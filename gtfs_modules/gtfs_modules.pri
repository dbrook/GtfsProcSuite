# Qxt Framework
#CONFIG += qxt
#QXT += core

INCLUDEPATH += $$PWD

DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/availableroutes.h \
    $$PWD/realtimestatus.h \
    $$PWD/realtimetripinformation.h \
    $$PWD/staticstatus.h \
    $$PWD/stationdetailsdisplay.h \
    $$PWD/stopsservedbyroute.h \
    $$PWD/stopswithouttrips.h \
    $$PWD/tripscheduledisplay.h \
    $$PWD/tripsservingroute.h \
    $$PWD/tripsservingstop.h \
    $$PWD/upcomingstopservice.h

SOURCES += \
    $$PWD/availableroutes.cpp \
    $$PWD/realtimestatus.cpp \
    $$PWD/realtimetripinformation.cpp \
    $$PWD/staticstatus.cpp \
    $$PWD/stationdetailsdisplay.cpp \
    $$PWD/stopsservedbyroute.cpp \
    $$PWD/stopswithouttrips.cpp \
    $$PWD/tripscheduledisplay.cpp \
    $$PWD/tripsservingroute.cpp \
    $$PWD/tripsservingstop.cpp \
    $$PWD/upcomingstopservice.cpp
