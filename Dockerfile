FROM ubuntu:24.04
SHELL ["/bin/bash", "-xec"]

ARG DEBIAN_FRONTEND=noninteractive
# Add LLVM APT repository for newer clang versions (clang-20)
RUN apt-get update;\
    apt-get install --no-install-recommends --yes ca-certificates wget;\
    mkdir -p /etc/apt/keyrings;\
    wget -qO /etc/apt/keyrings/llvm-snapshot.asc https://apt.llvm.org/llvm-snapshot.gpg.key;\
    echo "deb [signed-by=/etc/apt/keyrings/llvm-snapshot.asc] https://apt.llvm.org/noble/ llvm-toolchain-noble-20 main" \
        > /etc/apt/sources.list.d/llvm-20.list;\
    apt-get update;\
    apt-get install --no-install-recommends --yes\
        clang-16\
        clang-18\
        clang-20\
        cmake\
        g++-10\
        g++-13\
        g++-14\
        gcc-10\
        gcc-13\
        gcc-14\
        gcovr\
        googletest\
        libbenchmark-dev\
        libboost-filesystem-dev\
        libboost-system-dev\
        make\
        ;\
    apt-get autoremove --purge --yes;\
    apt-get clean

ENV GTEST_ROOT=/usr/src/googletest

WORKDIR /home/source
VOLUME ["/home/source"]
