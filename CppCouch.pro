TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CXXFLAGS += -std=c++14

INCLUDEPATH += ../../Repos/cpp-netlib
INCLUDEPATH += ./Boost

unix:!macx {
    LIBS += -lssl -lcrypto -lPocoFoundation -lPocoNet -lPocoNetSSL
}

SOURCES += main.cpp

HEADERS += \
    Couch/Cloudant/cloudantconnection.h \
    Couch/Cloudant/cloudantdatabase.h \
    Couch/Design/designdocument.h \
    Couch/Design/view.h \
    Couch/Design/viewinformation.h \
    Couch/attachment.h \
    Couch/changes.h \
    Couch/communication.h \
    Couch/connection.h \
    Couch/database.h \
    Couch/document.h \
    Couch/locator.h \
    Couch/replication.h \
    Couch/revision.h \
    Couch/shared.h \
    Couch/user.h \
    Couch/uuid.h \
    Network/network.h \
    Base64/base64.h \
    String/string_tools.h \
    Couch/cluster_connection.h \
    Couch/node_connection.h \
    Couch/cppcouch.h \
    Couch/cppcouchcloudant.h \
    json.h

DISTFILES += \
    todo.txt
