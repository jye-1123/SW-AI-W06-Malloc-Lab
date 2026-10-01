FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-multilib \
    gdb \
    make \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work