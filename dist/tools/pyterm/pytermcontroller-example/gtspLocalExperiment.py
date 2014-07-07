#!/usr/bin/python2
# -*- coding: utf-8 -*-

import sys, os
sys.path.append(os.path.join(os.path.dirname(__file__), "../pytermcontroller"))
sys.path.append(os.path.join(os.path.dirname(__file__), "../testbeds"))

from pytermcontroller import Experiment, ExperimentRunner
from testbeds import LocalTestbed

serverHost =  "localhost"
serverPort =  1025
basePath =    "/home/philipp"
flasher =     basePath + "/bin/n2n_flash.py"
hexFilePath = basePath + "/RIOT/examples/timesync/bin/avsextrem/timesync.hex"
pyterm =      basePath + "/RIOT/dist/tools/pyterm/pyterm.py -s " + serverHost + " -P " + str(serverPort) 
logFilePath = basePath + "/.pyterm/log"

class GTSPExperiment(Experiment): 
    def preHook(self):
        devlist = os.listdir("/dev/")        
        regex = re.compile('^ttyUSB')        
        portList = sorted([port for port in devlist if regex.match(port)])
        address = 1
        for port in portList:
            self.hostid[port] = address
            address += 1
      
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
        self.sendToAll("clocksynce heartbeat on")
        
    def enableGTSP(self):
        self.sendToAll("gtsp on")
        
    def disableGTSP(self):
        self.sendToAll("gtsp off")        
        
    def postHook(self): 
        pass
        #self.sendToAll("/exit")        
               
testbed = LocalTestbed(serverHost, serverPort, flasher, hexFilePath
                       ,pyterm, logFilePath)

testbed.flashNodes()
experiment = ExperimentRunner(GTSPExperiment, testbed) 
experiment.run()
