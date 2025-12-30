# Use Ubuntu 22.04 as the base OS
FROM ubuntu:22.04

# Avoid timezone prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libsqlite3-dev \
    netcat \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory inside the container
WORKDIR /app

# Copy source code files into the container
COPY . .

# Create a build directory and compile the project
RUN mkdir build && cd build && cmake .. && make

# By default, run the server (can be overridden in docker-compose)
CMD ["./build/tcp_server"]