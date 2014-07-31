#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import sys, os, re, datetime
sys.path.append(os.path.join(os.path.dirname(__file__), "../pytermcontroller"))
sys.path.append(os.path.join(os.path.dirname(__file__), "../testbeds"))

from pytermcontroller import Experiment, ExperimentRunner
from testbeds import LocalTestbed
from clocksyncExperiment import LocalClockSyncExperiment

serverHost =  "localhost"
serverPort =  1025
basePath =    "/home/philipp"
flasher =     basePath + "/RIOT/boards/msba2-common/tools/n2n_flash.py"
hexFilePath = basePath + "/RIOT/examples/clocksync/bin/avsextrem/clocksync.hex"
pyterm =      basePath + "/RIOT/dist/tools/pyterm/pyterm.py -s " + serverHost + " -P " + str(serverPort) 
logFilePath = basePath + "/.pyterm/log"

class GTSPLocalExperiment(LocalClockSyncExperiment):
    def enableProtocol(self):
        #self.sendToAll("gtsp delay 2240")
        self.sendToAll("gtsp delay 2220")
        #self.sendToAll("gtsp freq 10")
        self.sendToAll("gtsp on")
        
    def disableProtocol(self):
        self.sendToAll("gtsp off")        
        
    def postHook(self):         
        self.runner.testbed.archiveLogs("gtsp-local")
        #self.sendToAll("/exit")        
               
testbed = LocalTestbed(serverHost, serverPort, flasher, hexFilePath
                       ,pyterm, logFilePath)
testbed.flashNodes()
experiment = ExperimentRunner(GTSPLocalExperiment, testbed) 
experiment.run()
