TEMPLATE = lib 
CONFIG += warn_on
TARGET = qmfclient
INSTALLS += target
CONFIG += link_pkgconfig

simulator:macx:target.path += $$QMF_INSTALL_ROOT/Library/Frameworks
else:target.path += $$QMF_INSTALL_ROOT/lib

DEFINES += QT_BUILD_QCOP_LIB QMF_INTERNAL
win32: {
    # QLocalSocket is broken on win32 prior to 4.5.2
    lessThan(QT_MAJOR_VERSION,5):lessThan(QT_MINOR_VERSION,6):lessThan(QT_PATCH_VERSION,2):DEFINES += QT_NO_QCOP_LOCAL_SOCKET
}

QT = core sql network

# accounts dependencies
PKGCONFIG += accounts-qt
QT += xml

symbian: {
    include(../../../symbianoptions.pri)

    contains(CONFIG, SYMBIAN_USE_DATA_CAGED_DATABASE) {
        DEFINES += SYMBIAN_USE_DATA_CAGED_DATABASE

        INCLUDEPATH += symbian \
                       ../../symbian/qmfdataclient

        PRIVATE_HEADERS += ../../symbian/qmfdataclient/qmfdataclientservercommon.h \
                           ../../symbian/qmfdataclient/qmfdatasession.h \
                           ../../symbian/qmfdataclient/qmfdatastorage.h \
                           symbian/sqldatabase.h \
                           symbian/sqlquery.h

        SOURCES += ../../symbian/qmfdataclient/qmfdatasession.cpp \
                   ../../symbian/qmfdataclient/qmfdatastorage.cpp \
                   symbian/sqldatabase.cpp \
                   symbian/sqlquery.cpp

        LIBS += -lsqldb
    }

    contains(CONFIG, SYMBIAN_USE_IPC_SOCKET) {
        DEFINES += SYMBIAN_USE_IPC_SOCKET

        INCLUDEPATH += symbian
        PRIVATE_HEADERS += symbian/qmfipcchannelclient/qmfipcchannelclientservercommon.h \
                           symbian/qmfipcchannelclient/qmfipcchannelsession.h \
                           symbian/qmfipcchannelclient/qmfipcchannel.h \
                           symbian/ipcsocket.h \
                           symbian/ipcserver.h

        SOURCES += symbian/qmfipcchannelclient/qmfipcchannelsession.cpp \
                   symbian/qmfipcchannelclient/qmfipcchannel.cpp \
                   symbian/ipcsocket.cpp \
                   symbian/ipcserver.cpp
    }

    contains(CONFIG, SYMBIAN_THREAD_SAFE_MAILSTORE) {
        DEFINES += SYMBIAN_THREAD_SAFE_MAILSTORE
    }

    INCLUDEPATH += $$APP_LAYER_SYSTEMINCLUDE

    TARGET.EPOCALLOWDLLDATA = 1
    TARGET.CAPABILITY = ALL -TCB
    TARGET.UID3 = 0x20034921
    LIBS += -lefsrv
    MMP_RULES += EXPORTUNFROZEN

    QMFClient.sources = $${TARGET}.dll
    QMFClient.path = /sys/bin
    DEPLOYMENT += QMFClient
}

DEPENDPATH += .

INCLUDEPATH += support

PUBLIC_HEADERS += qmailaccount.h \
                  qmailaccountconfiguration.h \
                  qmailaccountkey.h \
                  qmailaccountlistmodel.h \
                  qmailaccountsortkey.h \
                  qmailaction.h \
                  qmailaddress.h \
                  qmailcodec.h \
                  qmailcontentmanager.h \
                  qmaildatacomparator.h \
                  qmaildisconnected.h \
                  qmailfolder.h \
                  qmailfolderfwd.h \
                  qmailfolderkey.h \
                  qmailfoldersortkey.h \
                  qmailid.h \
                  qmailkeyargument.h \
                  qmailmessage.h \
                  qmailmessagefwd.h \
                  qmailmessagekey.h \
                  qmailmessagelistmodel.h \
                  qmailmessagemodelbase.h \
                  qmailmessageremovalrecord.h \
                  qmailmessageserver.h \
                  qmailmessageset.h \
                  qmailmessagesortkey.h \
                  qmailmessagethreadedmodel.h \
                  qmailserviceaction.h \
                  qmailsortkeyargument.h \
                  qmailstore.h \
                  qmailtimestamp.h \
                  qmailthread.h \
                  qmailthreadkey.h \
                  qmailthreadlistmodel.h \
                  qmailthreadsortkey.h \
                  qprivateimplementation.h \
                  qprivateimplementationdef.h \
                  support/qmailglobal.h \
                  support/qmaillog.h \
                  support/qlogsystem.h \
                  support/qloggers.h \
                  support/qmailnamespace.h \
                  support/qmailpluginmanager.h \
                  support/qmailipc.h

PRIVATE_HEADERS += bind_p.h \
                   locks_p.h \
                   mailkeyimpl_p.h \
                   mailsortkeyimpl_p.h \
                   qmailaccountkey_p.h \
                   qmailaccountsortkey_p.h \
                   qmailfolderkey_p.h \
                   qmailfoldersortkey_p.h \
                   qmailmessage_p.h \
                   qmailmessagekey_p.h \
                   qmailmessageset_p.h \
                   qmailmessagesortkey_p.h \
                   qmailserviceaction_p.h \
                   qmailstore_p.h \
                   qmailstoreimplementation_p.h \
                   qmailthread_p.h \
                   qmailthreadkey_p.h \
                   qmailthreadsortkey_p.h \
                   longstring_p.h \
                   longstream_p.h \
                   support/qcopchannel_p.h \
                   support/qringbuffer_p.h \
                   support/qcopadaptor.h \
                   support/qcopapplicationchannel.h \
                   support/qcopchannel.h \
                   support/qcopchannelmonitor.h \
                   support/qcopserver.h \
                   ssoaccountmanager.h

PUBLIC_HEADERS += include/*

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS


SOURCES += longstream.cpp \
           longstring.cpp \
           qmailaccount.cpp \
           qmailaccountconfiguration.cpp \
           qmailaccountkey.cpp \
           qmailaccountlistmodel.cpp \
           qmailaccountsortkey.cpp \
           qmailaction.cpp \
           qmailaddress.cpp \
           qmailcodec.cpp \
           qmailcontentmanager.cpp \
           qmaildatacomparator.cpp \
           qmaildisconnected.cpp \
           qmailfolder.cpp \
           qmailfolderfwd.cpp \
           qmailfolderkey.cpp \
           qmailfoldersortkey.cpp \
           qmailid.cpp \
           qmailinstantiations.cpp \
           qmailkeyargument.cpp \
           qmailmessage.cpp \
           qmailmessagefwd.cpp \
           qmailmessagekey.cpp \
           qmailmessagelistmodel.cpp \
           qmailmessagemodelbase.cpp \
           qmailmessageremovalrecord.cpp \
           qmailmessageserver.cpp \
           qmailmessageset.cpp \
           qmailmessagesortkey.cpp \
           qmailmessagethreadedmodel.cpp \
           qmailserviceaction.cpp \
           qmailstore.cpp \
           qmailstore_p.cpp \
           qmailstoreimplementation_p.cpp \
           qmailtimestamp.cpp \
           qmailthread.cpp \
           qmailthreadkey.cpp \
           qmailthreadlistmodel.cpp \
           qmailthreadsortkey.cpp \
           qprivateimplementation.cpp \
           support/qmailnamespace.cpp \
           support/qmaillog.cpp \
           support/qlogsystem.cpp \
           support/qloggers.cpp \
           support/qcopadaptor.cpp \
           support/qcopapplicationchannel.cpp \
           support/qcopchannel.cpp \
           support/qcopchannelmonitor.cpp \
           support/qcopserver.cpp \
           support/qmailpluginmanager.cpp \
           ssoaccountmanager.cpp

win32: {
    SOURCES += locks_win32.cpp
} else {
    SOURCES += locks.cpp
}

RESOURCES += qmf.qrc \
             qmf_icons.qrc \
             qmf_qt.qrc

TRANSLATIONS += libqtopiamail-ar.ts \
                libqtopiamail-de.ts \
                libqtopiamail-en_GB.ts \
                libqtopiamail-en_SU.ts \
                libqtopiamail-en_US.ts \
                libqtopiamail-es.ts \
                libqtopiamail-fr.ts \
                libqtopiamail-it.ts \
                libqtopiamail-ja.ts \
                libqtopiamail-ko.ts \
                libqtopiamail-pt_BR.ts \
                libqtopiamail-zh_CN.ts \
                libqtopiamail-zh_TW.ts

header_files.path=$$QMF_INSTALL_ROOT/include/qmfclient
header_files.files=$$PUBLIC_HEADERS

INSTALLS += header_files

symbian {
    for(header, header_files.files) {
        BLD_INF_RULES.prj_exports += "$$header $$MW_LAYER_PUBLIC_EXPORT_PATH("qmf/"$$basename(header))"
    }
}

# Install generic SSO provider description
sso_providers.files = share/GenericProvider.provider
sso_providers.path  = $$QMF_INSTALL_ROOT/share/accounts/providers

# Install generic SSO service description
sso_services.files = share/GenericEmail.service
sso_services.path  = $$QMF_INSTALL_ROOT/share/accounts/services

INSTALLS += sso_providers sso_services

unix: {
	CONFIG += create_pc create_prl
	QMAKE_PKGCONFIG_LIBDIR  = $$target.path
	QMAKE_PKGCONFIG_INCDIR  = $$header_files.path
	QMAKE_PKGCONFIG_DESTDIR = pkgconfig
        LIBS += -licui18n -licuuc -licudata
        PRIVATE_HEADERS += support/qcharsetdetector_p.h \
                           support/qcharsetdetector.h
        SOURCES += support/qcharsetdetector.cpp
        DEFINES += HAVE_LIBICU
}

include(../../../common.pri)
