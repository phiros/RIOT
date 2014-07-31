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

class FTSPLocalExperiment(LocalClockSyncExperiment):
    def enableProtocol(self):
        self.sendToAll("ftsp on")
        
    def disableProtocol(self):
        self.sendToAll("ftsp off")        
        
    def postHook(self):         
        self.runner.testbed.archiveLogs("ftsp-local")
        #self.sendToAll("/exit")        
               
testbed = LocalTestbed(serverHost, serverPort, flasher, hexFilePath
                       ,pyterm, logFilePath)
testbed.flashNodes()
experiment = ExperimentRunner(FTSPLocalExperiment, testbed) 
experiment.run()
