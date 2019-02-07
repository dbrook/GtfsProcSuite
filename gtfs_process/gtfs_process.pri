# Qxt Framework
#CONFIG += qxt
#QXT += core

INCLUDEPATH += $$PWD

DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/gtfsstatus.h \
    $$PWD/csvprocessor.h \
    $$PWD/datagateway.h \
    $$PWD/gtfsroute.h \
    $$PWD/operatingday.h \
    $$PWD/gtfstrip.h \
    $$PWD/gtfsstoptimes.h \
    $$PWD/gtfsstops.h \
    $$PWD/gtfsfrequencies.h

SOURCES += \
    $$PWD/gtfsstatus.cpp \
    $$PWD/csvprocessor.cpp \
    $$PWD/datagateway.cpp \
    $$PWD/gtfsroute.cpp \
    $$PWD/operatingday.cpp \
    $$PWD/gtfstrip.cpp \
    $$PWD/gtfsstoptimes.cpp \
    $$PWD/gtfsstops.cpp \
    $$PWD/gtfsfrequencies.cpp

LIBS += -lcsv
