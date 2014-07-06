#!/usr/bin/python2
# -*- coding: utf-8 -*-

import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), "../pytermcontroller"))
sys.path.append(os.path.join(os.path.dirname(__file__), "../testbeds"))

from pytermcontroller import Experiment, ExperimentRunner
from testbeds import DESTestbed

serverHost =  "uhu"
serverPort =  1025
userName =    "phiros"
basePath =    "/home/phiros"
flasher =     basePath + "/bin/n2n_flash.py"
hexFilePath = basePath + "/toflash.hex"
pyterm =      basePath + "/RIOT/dist/tools/pyterm/pyterm.py" 
logFilePath = basePath + "/testbed/.pyterm/log"
hostFile =    basePath + "/testbed/hosts"

class GTSPExperiment(Experiment): 
    def preHook(self):
        self.readHostFile(hostFile)
      
    def start(self):
        self.waitAndCall(1*60,   self.setup)
        self.waitAndCall(15*60 , self.enableGTSP)
        self.waitAndCall(15*60 , self.disableGTSP)
        self.waitAndCall(15*60 , self.stop)     
        
    def setup(self):
        for host, connection in self.clientIterator():
            if self.hostid[host]:
                self.sendToConnection(connection, "addr " + str(self.hostid[host]))
        
        self.sendToAll("clocksynce beacon interval 5000 5000")
        self.sendToAll("clocksynce beacon on")
        self.sendToAll("clocksynce beacon heartbeat on")
        
    def enableGTSP(self):
        self.sendToAll("gtsp on")
        
    def disableGTSP(self):
        self.sendToAll("gtsp off")        
        
    def postHook(self): 
        pass
        #self.sendToAll("/exit")        
               
testbed = DESTestbed(serverHost, serverPort, userName, flasher, 
                     hexFilePath, pyterm, logFilePath, hostFile)
testbed.flashNodes()
experiment = ExperimentRunner(GTSPExperiment, testbed) 
experiment.run()
