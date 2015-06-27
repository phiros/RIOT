#
# RIOT Dockerfile
#
# the resulting image will contain everything needed to build RIOT.
#
# Setup: (only needed once per Dockerfile change)
# 1. install docker, add yourself to docker group, enable docker, relogin
# 2. # docker build -t riotbuild .
#
# Usage:
# 3. cd to riot root
# 4. # docker run -i -t -u $UID -v $(pwd):/data/riotbuild riotbuild ./dist/tools/compile_test/compile_test.py
#
# If you want to use a persistent ccache, map a directory to '/data/ccache'
# and set CCACHE=ccache:
#
# 4. # docker run -i -t -u $UID -v $(pwd):/data/riotbuild -v /tmp/riot_ccache:/data/ccache \
#           -e CCACHE=ccache -e RIOT_VERSION_OVERRIDE=buildtest \
#            riotbuild ./dist/tools/compile_test/compile_test.py
#

#FROM strider/strider-docker-slave
FROM strider-docker-slave

MAINTAINER Philipp Rosenkranz <philipp.rosenkranz@fu-berlin.de>

ENV DEBIAN_FRONTEND noninteractive

USER root

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y -q python-software-properties software-properties-common

RUN add-apt-repository ppa:terry.guo/gcc-arm-embedded

RUN apt-get update
RUN apt-get -y dist-upgrade

# native platform development and build system functionality (about 400 MB installed)
RUN apt-get -y install \
    build-essential \
    curl \
    cppcheck \
    doxygen \
    gcc-multilib \
    g++-multilib \
    git \
    graphviz \
    libpcre3 \
    parallel \
    pcregrep \
    python \
    python3 \
    p7zip \
    subversion \
    unzip \
    wget

# Cortex-M development (about 550 MB installed)
RUN apt-get -y install \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libnewlib-dev

# MSP430 development (about 120 MB installed)
RUN apt-get -y install \
    gcc-msp430

# AVR development (about 110 MB installed)
RUN apt-get -y install \
    gcc-avr \
    binutils-avr \
    avr-libc

# LLVM/Clang build environment (about 125 MB installed)
RUN apt-get -y install \
    llvm

# x86 bare metal emulation (about 125 MB installed) (this pulls in all of X11)
RUN apt-get -y install \
    qemu-system-x86

# Hack for non reachable sites
RUN apt-get -y install dnsmasq

ADD dnsmasq.conf /etc/dnsmasq.conf
ADD dhosts /etc/dhosts 
ADD supervisor-dnsmasq.conf /etc/supervisor/conf.d/

RUN wget http://launchpadlibrarian.net/206632429/ccache_3.2.2-2_amd64.deb \
        -O /tmp/ccache_3.2.2-2_amd64.deb \
        && dpkg -i /tmp/ccache_3.2.2-2_amd64.deb

RUN mkdir -m 777 -p /data/ccache
ENV CCACHE_DIR /data/ccache

#USER strider
