FROM ubuntu:jammy
SHELL ["/bin/bash", "-xec"]
RUN export DEBIAN_FRONTEND=noninteractive;\
    apt-get update;\
    apt-get dist-upgrade --purge --yes\
        bind9-dnsutils\
        cmake\
        g++-12\
        gdb\
        googletest\
        iptables\
        iproute2\
        jq\
        lcov\
        libnetfilter-queue-dev\
        libnfnetlink-dev\
        lsof\
        net-tools\
        ninja-build\
        openssh-server\
        pkg-config\
        python3-pip\
        python3-scapy\
        tcpdump\
        valgrind\
        socat \
        strace \
        ;\
    apt-get autoremove --purge --yes;\
    apt-get clean;\
    pip3 install gcovr NetfilterQueue

# Pinned GCC version
ENV CC=gcc-12
ENV CXX=g++-12
ENV GCOV=gcov-12

# Build boost
ARG BOOST_MAJOR
ARG BOOST_MINOR
ARG BOOST_PATCH

RUN wget https://archives.boost.io/release/${BOOST_MAJOR}.${BOOST_MINOR}.${BOOST_PATCH}/source/boost_${BOOST_MAJOR}_${BOOST_MINOR}_${BOOST_PATCH}.tar.gz -O boost_sources.tar.gz;\
    tar -zxvf boost_sources.tar.gz;\
    cd boost_${BOOST_MAJOR}_${BOOST_MINOR}_${BOOST_PATCH};\
    ./bootstrap.sh --prefix=/usr;\
    ./b2 install valgrind=on --with-system --with-log --with-filesystem --with-thread

# /etc configurations
RUN echo 'SendEnv *' > /etc/ssh/ssh_config.d/vsomeip.conf;\
    echo 'AcceptEnv *' > /etc/ssh/sshd_config.d/vsomeip.conf

# SSH configurations
RUN ssh-keygen -q -b 1024 -f ~/.ssh/id_rsa -t rsa -N '';\
    install -DTm0400 ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys;\
    install -DTm0400 ~/.ssh/id_rsa /commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub

# Add ssh_config to the container
COPY vsomeip-lib/zuul/network-tests/ssh_config /root/.ssh/config
RUN chown root:root /root/.ssh/config && chmod 600 /root/.ssh/config

# Hard-coded path used in the tests' scripts
RUN ln -s /home/build /vsomeip_lib

# Place stuff in a known place
WORKDIR /home/source

# Declare bind-mounts
VOLUME ["/home/build", "/home/logs", "/home/source"]
