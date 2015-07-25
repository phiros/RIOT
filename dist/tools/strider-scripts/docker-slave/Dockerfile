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

FROM riot/riotbuild:latest

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update

# Node.js and Git are required
RUN curl -sL https://deb.nodesource.com/setup_0.12 | bash -
RUN apt-get install -y nodejs git
#RUN apt-get -y install nodejs npm git
#RUN update-alternatives --install /usr/bin/node node /usr/bin/nodejs 10

# git wants this
RUN locale-gen en_US.UTF-8

# Setup workspace and user
RUN adduser --home /home/strider --gecos "" strider
RUN mkdir -p /home/strider/workspace
RUN chown -R strider /home/strider

# Get the slave
RUN npm install -g strider-docker-slave@1.*.*

# Install supervisord
RUN apt-get -y install supervisor && \
  mkdir -p /var/log/supervisor && \
  mkdir -p /etc/supervisor/conf.d

# Run the slave
# Additional background services can be configured by adding
# a supervisor config file to the config directory 
# (/etc/supervisor/conf.d/)
CMD supervisord -c /etc/supervisor/supervisord.conf && su strider -c 'strider-docker-slave'

# Hack for non reachable sites
RUN apt-get -y install dnsmasq supervisor

ADD dnsmasq.conf /etc/dnsmasq.conf
ADD dhosts /etc/dhosts 
ADD supervisor-dnsmasq.conf /etc/supervisor/conf.d/

RUN wget http://launchpadlibrarian.net/206632429/ccache_3.2.2-2_amd64.deb \
        -O /tmp/ccache_3.2.2-2_amd64.deb \
        && dpkg -i /tmp/ccache_3.2.2-2_amd64.deb

RUN mkdir -m 777 -p /data/ccache
ENV CCACHE_DIR /data/ccache

WORKDIR /home/strider/workspace
ENV HOME /home/strider
