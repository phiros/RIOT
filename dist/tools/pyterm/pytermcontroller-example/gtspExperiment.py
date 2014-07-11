#!/usr/bin/python2
# -*- coding: utf-8 -*-

import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), "../pytermcontroller"))
sys.path.append(os.path.join(os.path.dirname(__file__), "../testbeds"))

from pytermcontroller import Experiment, ExperimentRunner
from testbeds import DESTestbed
from clocksyncExperiment import ClockSyncExperiment


serverHost =  "uhu"
serverPort =  1025
userName =    "phiros"
basePath =    "/home/phiros"
flasher =     basePath + "/bin/n2n_flash.py"
hexFilePath = basePath + "/toflash.hex"
pyterm =      basePath + "/RIOT/dist/tools/pyterm/pyterm.py -s " + serverHost + " -P " + str(serverPort) 
logFilePath = basePath + "/testbed/.pyterm/log"
hostFile =    basePath + "/testbed/hosts"

class GTSPExperiment(ClockSyncExperiment): 
    def preHook(self):
        self.readHostFile(hostFile)      
            
    def enableProtocol(self):
        self.sendToAll("gtsp delay 2220")
        self.sendToAll("gtsp on")
        
    def disableProtocol(self):
        self.sendToAll("gtsp off")        
        
    def postHook(self): 
        self.runner.testbed.archiveLogs("gtsp-des")
        #self.sendToAll("/exit")        
               
testbed = DESTestbed(serverHost, serverPort, userName, flasher, 
                     hexFilePath, pyterm, logFilePath, hostFile)
testbed.flashNodes()
experiment = ExperimentRunner(GTSPExperiment, testbed) 
experiment.run()
