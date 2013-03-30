TEMPLATE = app
TARGET = qmftool
CONFIG += qmfclient
QT = core
target.path += $$QMF_INSTALL_ROOT/bin
INCLUDEPATH += . \
               ../../libraries/qmfclient \
               ../../libraries/qmfclient/support
LIBS += -L../../libraries/qmfclient/build
macx:LIBS += -F../../libraries/qmfclient/build

# Input
HEADERS += qmftool.h logging.h
SOURCES += qmftool.cpp logging.cpp main.cpp

include(../../../common.pri)
