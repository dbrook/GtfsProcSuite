# Qxt Framework
#CONFIG += qxt
#QXT += core

INCLUDEPATH += $$PWD

DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/gtfsrealtimegateway.h\
    $$PWD/gtfs-realtime.pb.h \
    $$PWD/gtfsrealtimefeed.h

SOURCES += \
    $$PWD/gtfsrealtimegateway.cpp\
    $$PWD/gtfs-realtime.pb.cc \
    $$PWD/gtfsrealtimefeed.cpp

LIBS += -lprotobuf -latomic
