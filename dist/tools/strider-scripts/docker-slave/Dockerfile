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

RUN apt-get -y install build-essential \
 git \
 gcc-multilib \
 gcc-arm-none-eabi \
 gcc-msp430 \
 pcregrep libpcre3 \
 qemu-system-x86 python3 \
 g++-multilib \
 gcc-avr binutils-avr avr-libc \
 subversion curl wget python p7zip unzip

RUN mkdir -p /data/riotbuild
WORKDIR /data/riotbuild

USER strider # set the user back to strider at the end
