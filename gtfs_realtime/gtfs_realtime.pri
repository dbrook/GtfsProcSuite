# GTFS-Realtime is delivered with protocol buffers, make sure we run protoc to generate C++ headers/sources
# Generate protocol buffer for GTFS-Realtime so we don't have cross-distribution / version issues at build- run-time
# Thanks to: https://vilimpoc.org/blog/2013/06/09/using-google-protocol-buffers-with-qmake/
PROTOS = $$PWD/gtfs-realtime.proto
for(p, PROTOPATH):PROTOPATHS += --proto_path=$${p}

protobuf_decl.name = protobuf headers
protobuf_decl.input = PROTOS
protobuf_decl.output = ${QMAKE_FILE_IN_PATH}/${QMAKE_FILE_BASE}.pb.h
protobuf_decl.commands = protoc --cpp_out=${QMAKE_FILE_IN_PATH} --proto_path=${QMAKE_FILE_IN_PATH} ${QMAKE_FILE_NAME}
protobuf_decl.variable_out = HEADERS
QMAKE_EXTRA_COMPILERS += protobuf_decl

protobuf_impl.name = protobuf sources
protobuf_impl.input = PROTOS
protobuf_impl.output = ${QMAKE_FILE_IN_PATH}/${QMAKE_FILE_BASE}.pb.cc
protobuf_impl.depends = ${QMAKE_FILE_IN_PATH}/${QMAKE_FILE_BASE}.pb.h
protobuf_impl.commands = $$escape_expand(\n)
protobuf_impl.variable_out = SOURCES
QMAKE_EXTRA_COMPILERS += protobuf_impl

# Wrapper /abstraction objects to avoid coding directly against protobuf files
INCLUDEPATH += $$PWD

DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/gtfsrealtimegateway.h\
    $$PWD/gtfsrealtimefeed.h
SOURCES += \
    $$PWD/gtfsrealtimegateway.cpp\
    $$PWD/gtfsrealtimefeed.cpp

# For Debian/Ubunto Linux:
#LIBS += -lprotobuf -latomic

# For Arch/Manjaro Linux:
LIBS += -lprotobuf -latomic -labsl_log_internal_check_op -labsl_log_internal_message -labsl_status

