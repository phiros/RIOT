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
hexFilePath = basePath + "/RIOT/examples/timesync/bin/avsextrem/timesync.hex"
pyterm =      basePath + "/RIOT/dist/tools/pyterm/pyterm.py -s " + serverHost + " -P " + str(serverPort) 
logFilePath = basePath + "/.pyterm/log"

class PulsesyncLocalExperiment(LocalClockSyncExperiment):
    def enableProtocol(self):
        self.sendToAll("pulsesync on")
        
    def disableProtocol(self):
        self.sendToAll("pulsesync off")        
        
    def postHook(self):         
        self.runner.testbed.archiveLogs("pulsesync-local")
        #self.sendToAll("/exit")        
               
testbed = LocalTestbed(serverHost, serverPort, flasher, hexFilePath
                       ,pyterm, logFilePath)
testbed.flashNodes()
experiment = ExperimentRunner(PulsesyncLocalExperiment, testbed) 
experiment.run()
