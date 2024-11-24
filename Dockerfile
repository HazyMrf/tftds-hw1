FROM --platform=linux/arm64 ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    iptables \
    iputils-ping \
    iproute2 \
    net-tools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /

COPY . .

RUN cmake . && make

ENV PORT=8080
ENV BROADCAST_PORT=8081