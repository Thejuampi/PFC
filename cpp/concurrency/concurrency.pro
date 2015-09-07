TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt
CONFIG += cl-std=CL2.0

SOURCES += main.cpp \
    oclutils.cpp

INCLUDEPATH += /opt/AMDAPPSDK-3.0/include/
INCLUDEPATH += /opt/AMDAPPSDK-3.0/include/SDKUtil/

LIBS += -L"/opt/AMDAPPSDK-3.0/lib/x86_64/sdk/" -lOpenCL

HEADERS += \
    oclutils.h

DISTFILES += \
    vecadd.cl
