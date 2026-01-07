FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    sqlite3 \
    pkg-config \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN rm -rf CMakeFiles CMakeCache.txt cmake_install.cmake Makefile

RUN set -x && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make

CMD ["./build/tcp_server"]