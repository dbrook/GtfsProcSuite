# Debian 12 with Qt 6 is the reference environment for GtfsProc
FROM debian:bookworm

# Install UTF8 English (US) because ... sure, why not?
RUN apt-get update && apt-get install -y locales && rm -rf /var/lib/apt/lists/* \
	&& localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8
ENV LANG en_US.utf8

# Install GtfsProc Qt build and refresher script's runtime requirements
RUN apt-get update && apt-get install -y build-essential qtchooser qmake6 qt6-base-dev qt6-connectivity-dev libcsv-dev protobuf-compiler libprotobuf-dev libncurses-dev perl-base unzip curl

WORKDIR /src

COPY . .

# Build GtfsProc (assumes mounting in the top-level src directory in the container)
RUN qtchooser -install qt6 $(which qmake6) \
    && protoc --version \
    && cat gtfs_realtime/gtfs-realtime.pb.h \
    && ls -la /usr/include/google/protobuf \
    && export QT_SELECT=qt6 \
    && qmake6 \
    && make \
    && ls -l gtfsproc

# Start GtfsProc using ...
CMD ["gtfsproc/gtfsproc", "-h"]

# Default port is 5000
EXPOSE 5000
