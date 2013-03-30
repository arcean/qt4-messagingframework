TEMPLATE = lib 
CONFIG += warn_on
CONFIG += qmfclient
TARGET = qmfmessageserver

target.path += $$QMF_INSTALL_ROOT/lib

QT = core network
!contains(DEFINES,QMF_NO_MESSAGE_SERVICE_EDITOR):QT += gui

symbian: {
    TARGET.EPOCALLOWDLLDATA = 1
    TARGET.CAPABILITY = ALL -TCB
    TARGET.UID3 = 0x20034922
    MMP_RULES += EXPORTUNFROZEN

    QMFServer.sources = $${TARGET}.dll
    QMFServer.path = /sys/bin
    DEPLOYMENT += QMFServer
}

DEFINES += MESSAGESERVER_INTERNAL

DEPENDPATH += .

INCLUDEPATH += . ../qmfclient ../qmfclient/support

LIBS += -L../qmfclient/build
macx:LIBS += -F../qmfclient/build

PUBLIC_HEADERS += qmailauthenticator.h \
                  qmailmessagebuffer.h \
                  qmailmessageclassifier.h \
                  qmailmessageservice.h \
                  qmailserviceconfiguration.h \
                  qmailstoreaccountfilter.h \
                  qmailtransport.h \
                  qmailheartbeattimer.h
PUBLIC_HEADERS += include/*

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

SOURCES += qmailauthenticator.cpp \
           qmailmessagebuffer.cpp \
           qmailmessageclassifier.cpp \
           qmailmessageservice.cpp \
           qmailserviceconfiguration.cpp \
           qmailstoreaccountfilter.cpp \
           qmailtransport.cpp

iphb: {
    SOURCES += qmailheartbeattimer_iphb.cpp
    CONFIG += mobility
    MOBILITY += systeminfo
} else {
    SOURCES += qmailheartbeattimer_qtimer.cpp
}

header_files.path=$$QMF_INSTALL_ROOT/include/qmfmessageserver
header_files.files=$$PUBLIC_HEADERS

INSTALLS += header_files

unix: {
	CONFIG += create_pc create_prl
	QMAKE_PKGCONFIG_LIBDIR  = $$target.path
	QMAKE_PKGCONFIG_INCDIR  = $$header_files.path
	QMAKE_PKGCONFIG_DESTDIR = pkgconfig
}
include(../../../common.pri)
