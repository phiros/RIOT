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

class PulsesyncExperiment(ClockSyncExperiment): 
    def preHook(self):
        self.readHostFile(hostFile)      
            
    def enableProtocol(self):
        self.sendToAll("pulsesync delay 2200")
        self.sendToAll("pulsesync on")
        
    def disableProtocol(self):
        self.sendToAll("pulsesync off")        
        
    def postHook(self): 
        self.runner.testbed.archiveLogs("pulsesync-des")
        #self.sendToAll("/exit")        
               
testbed = DESTestbed(serverHost, serverPort, userName, flasher, 
                     hexFilePath, pyterm, logFilePath, hostFile)
testbed.flashNodes()
experiment = ExperimentRunner(PulsesyncExperiment, testbed) 
experiment.run()
