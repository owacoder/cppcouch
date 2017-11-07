#ifndef CPPCOUCH_CHANGES_H
#define CPPCOUCH_CHANGES_H

#include "communication.h"
#include "connection.h"
#include <json.h>

#include <mutex>
#include <thread>

namespace couchdb
{
    /* signal_base class - This class is a base class for changes-feed signal handlers.
     * Any subclasses should not touch the firing changes object, as this could lead to a deadlock
     * with mutexes. Subclasses must reimplement the change_occured() member. The convenience functions
     * changes_feed_opened() and changes_feed_closed() fire when the feed opens and closes respectively.
     *
     * The signals are fired in the context of the changes-feed thread, not the main thread
     * (unless the main thread is the changes-feed thread).
     *
     * Usage from the main thread MUST attempt to lock the mutex member before use.
     *
     * NOTE: this class is not copyable, since it has a std::mutex member. Subclasses must take that into
     * consideration.
     */
    class signal_base
    {
        std::mutex mutex_;

    public:
        virtual ~signal_base() {}

        std::mutex &mutex() {return mutex_;}

        virtual void changes_feed_opened() {}
        virtual void change_occured(const json::value &change) = 0;
        virtual void changes_feed_closed() {}

        bool try_lock() {return mutex_.try_lock();}
        void lock() {mutex_.lock();}
        void unlock() {mutex_.unlock();}
    };

    /* changes_feed_thread class - This class is a high-level changes-feed handler, and manages a separate thread
     * for watching changes. Begin a new feed with start_in_other_thread(), which issues requests entirely in a new
     * thread, or use start_in_this_thread(), followed by run_in_other_thread() to connect in the current thread and
     * watch for changes in the managed thread.
     *
     * To stop the feed, use stop() (blocking) or try_stop() (non-blocking, returns false on failure) which just stop the feed,
     * not the managed thread. To stop the feed AND managed thread, use stop_and_wait_for_finish() to ensure the
     * process is entirely shut down, or use stop_and_detach() to stop the process and detach the thread to finish on its own.
     *
     * NOTE: this class is not copyable, since the signaller, thread, and changes classes are not copyable.
     *
     * WARNING: this class uses a copy of the communication object used by the database parameter. Changing settings on the database's
     * communication object will NOT affect this changes feed. Thus, the required settings should be set up prior to creating a change-feed,
     * and then set back to their prior state, if necessary, afterward.
     *
     * However, the communication_editor class exists to provide a mutex-lockable method of viewing or editing communication settings on an existing feed.
     * Simply create a communication_editor, with the changes feed as a parameter, and then use the communication() function to change settings.
     * Change signals will be frozen while the communication_editor exists. To unfreeze signals and apply changes to the communication object,
     * destroy the communication_editor.
     */
    template<typename http_client, typename signal_type>
    class changes_feed_thread
    {
    public:
        std::shared_ptr<typename changes<http_client, signal_type>::communication_editor> make_communication_editor()
        {
            return std::make_shared<typename changes<http_client, signal_type>::communication_editor>(changes_feed);
        }

        typedef database<http_client> database_type;

        changes_feed_thread(database_type database) : changes_feed(database, signaller) {}

        database_type get_db() {return changes_feed.get_db();}

        bool is_active() const {return changes_feed.is_active();}
        void start_in_this_thread(const queries &q) {changes_feed.start(q);}
        void start_and_run_in_other_thread(const queries &q) {changes_feed.start_in_thread(thread, q);}
        void run_in_this_thread() {changes_feed.run_in_this_thread();}
        void run_in_other_thread() {changes_feed.run_in_thread(thread);}
        void stop() {changes_feed.stop();}
        bool try_stop() {return changes_feed.try_stop();}

        void stop_and_wait_for_finish()
        {
            stop();
            if (thread.joinable())
                thread.join();
        }
        void stop_and_detach()
        {
            stop();
            if (thread.joinable())
                thread.detach();
        }

        bool error_was_raised() const {return changes_feed.error_was_raised();}
        error last_error() const {return changes_feed.last_error();}

    private:
        signal_type signaller;
        std::thread thread;
        changes<http_client, signal_type> changes_feed;
    };

    /* changes class - This class opens a connection to CouchDB in the "continuous" feed mode,
     * When a change is made in CouchDB, this class will emit the signal change_occured() with CouchDB's update.
     * When the feed is stopped by stop(), the changes_feed_closed() signal will be emitted.
     * This class should not be touched by the signal handler.
     */
    template<typename http_client, typename signal_type>
    class changes
    {
        changes(const changes &) {}
        void operator=(const changes &) {}

        typedef connection<http_client> connection_type;
        typedef communication<http_client> communication_type;
        typedef database<http_client> database_type;

        mutable std::mutex comm_mutex, err_mutex;

    public:
        class communication_editor
        {
            changes *feed;

            communication_editor(const communication_editor &) = delete;
            communication_editor(communication_editor &&) = delete;
            communication_editor &operator=(const communication_editor &) = delete;
            communication_editor &operator=(communication_editor &&) = delete;

        public:
            communication_editor(changes &feed_ptr) : feed(&feed_ptr) {feed->comm_mutex.lock();}
            ~communication_editor() {feed->comm_mutex.unlock();}

            communication_type &communication() {return *feed->comm;}
        };

        std::shared_ptr<communication_editor> make_communication_editor()
        {
            return std::make_shared<communication_editor>(*this);
        }

        changes(database_type database, signal_type &signaller)
            : db(database)
            , signaller(signaller)
            , comm(std::make_shared<communication_type>(http_client(database.get_connection().lowest_level().get_client())))
            , handle(comm->get_client().invalid_handle())
            , stop_requested(false)
            , has_error(false)
            , err(error::unknown_error)
        {
            comm->set_current_state(database.get_connection().lowest_level().get_current_state());
        }

        // Returns the connection object of this changes feed
        database_type get_db() {return db;}

        // Returns true if this changes feed is active (i.e. connected and watching for changes)
        bool is_active() const
        {
            std::lock_guard<std::mutex> lock(comm_mutex);
            return !comm->get_client().is_invalid_handle(handle) && !stop_requested;
        }

        // Starts a continuous feed with the provided queries in the current thread
        void start(const queries &options = queries())
        {
            std::lock_guard<std::mutex> lock(comm_mutex);
            if (comm->get_client().is_invalid_handle(handle))
            {
                std::string url = "/" + url_encode(db.get_db_name()) + "/_changes?feed=continuous";
                handle = comm->get_raw_data_response(add_url_queries(url, options));

                {
                    std::lock_guard<std::mutex> lock(signaller.mutex());
                    signaller.changes_feed_opened();
                }
            }
        }

        // This function never returns unless the feed is shutdown externally (i.e. not by this process), or by an error
        void run_in_this_thread() {run(this);}

        // Waits for a single change or heartbeat signal to arrive
        void wait_for_changes()
        {
            std::string line;

            {
                std::lock_guard<std::mutex> lock(comm_mutex);
                line = comm->get_client().read_line_from_response_handle(handle);
            }

            if (!line.empty())
            {
                std::lock_guard<std::mutex> lock(signaller.mutex());
                signaller.change_occured(string_to_json(line));
            }
        }

        // Starts and runs a continuous feed with the provided queries in a new thread and returns the new thread
        std::shared_ptr<std::thread> start_in_new_thread(const queries &q)
        {
            std::shared_ptr<std::thread> thread = std::make_shared<std::thread>();

            std::thread(start_and_run, this, q).swap(*thread);
            return thread;
        }

        // Starts and runs a continuous feed with the provided queries in a new thread and assigns it to the thread parameter
        void start_in_thread(std::thread &thread, const queries &q)
        {
            std::thread(start_and_run, this, q).swap(thread);
        }

        // Runs a continuous feed with the provided queries in a new thread and assigns it to the thread parameter
        // The changes feed is assumed to be start()ed already, if it is not, the thread will finish immediately.
        void run_in_thread(std::thread &thread)
        {
            std::thread(run, this).swap(thread);
        }

        // Returns the client response handle being used for the changes feed, or the invalid_handle() specified by the client
        typename http_client::response_handle_type get_response_handle() const {return handle;}

        // Stops the current feed (not the thread it operates in),
        // blocking until the feed is shut down completely or has been requested to shut down
        void stop()
        {
            std::lock_guard<std::mutex> lock(comm_mutex);
            if (!comm->get_client().is_invalid_handle(handle))
            {
                comm->get_client().reset();
                handle = comm->get_client().invalid_handle();

                {
                    std::lock_guard<std::mutex> lock(signaller.mutex());
                    signaller.changes_feed_closed();
                }
            }
            else
                stop_requested = true;
        }

        // Same as stop(), but if it cannot request a stop immediately, will return false.
        // Returns true if the feed was shut down or has been requested to shut down.
        bool try_stop()
        {
            if (comm_mutex.try_lock())
            {
                std::lock_guard<std::mutex> lock(comm_mutex, std::adopt_lock);
                if (!comm->get_client().is_invalid_handle(handle))
                {
                    comm->get_client().reset();
                    handle = comm->get_client().invalid_handle();

                    {
                        std::lock_guard<std::mutex> lock(signaller.mutex());
                        signaller.changes_feed_closed();
                    }
                }
                else
                    stop_requested = true;

                return true;
            }
            return false;
        }

        bool error_was_raised() const
        {
            std::lock_guard<std::mutex> lock(err_mutex);
            return has_error;
        }

        error last_error() const
        {
            std::lock_guard<std::mutex> lock(err_mutex);
            return err;
        }

    protected:
        static void start_and_run(changes *changes_feed, const queries &q)
        {
            try
            {
                changes_feed->start(q);
                std::this_thread::yield();
                run(changes_feed);
            } catch (couchdb::error e)
            {
                std::lock_guard<std::mutex> lock(changes_feed->err_mutex);
                changes_feed->has_error = true;
                changes_feed->err = e;
            }

            {
                std::unique_lock<std::mutex> lock(changes_feed->comm_mutex);
                if (changes_feed->stop_requested)
                {
                    lock.unlock();
                    changes_feed->stop();
                    lock.lock();
                }
                changes_feed->stop_requested = false;
            }
        }

        static void run(changes *changes_feed)
        {
            try
            {
                while (changes_feed->is_active())
                {
                    changes_feed->wait_for_changes();
                    std::this_thread::yield();
                }
            } catch (couchdb::error e)
            {
                std::lock_guard<std::mutex> lock(changes_feed->err_mutex);
                changes_feed->has_error = true;
                changes_feed->err = e;
            }

            {
                std::unique_lock<std::mutex> lock(changes_feed->comm_mutex);
                if (changes_feed->stop_requested)
                {
                    lock.unlock();
                    changes_feed->stop();
                    lock.lock();
                }
                changes_feed->stop_requested = false;
            }
        }

    private:
        database_type db;
        signal_type &signaller; // Protected by its own mutex in signal_base class
        std::shared_ptr<communication_type> comm; // Protected by comm_mutex
        typename http_client::response_handle_type handle; // Protected by comm_mutex
        bool stop_requested; // Protected by comm_mutex

        bool has_error; // Protected by err_mutex
        error err; // Protected by err_mutex
    };
}

#endif // CPPCOUCH_CHANGES_H
