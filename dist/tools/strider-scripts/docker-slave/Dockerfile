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

FROM strider/strider-docker-slave

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

RUN printf "127.0.0.1   tlsf.baisoku.org\n127.0.0.1   olsr.org\n\
127.0.0.1   libfixmath.googlecode.com\n127.0.0.1   git.code.sf.net" >> /etc/hosts

USER strider
