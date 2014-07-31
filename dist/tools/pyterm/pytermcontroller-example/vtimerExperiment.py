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
hexFilePath = basePath + "/RIOT/examples/test-gtimer/bin/avsextrem/test-gtimer.hex"
pyterm =      basePath + "/RIOT/dist/tools/pyterm/pyterm.py -s " + serverHost + " -P " + str(serverPort) 
logFilePath = basePath + "/.pyterm/log"

class VtimerExperiment(Experiment):
    def start(self):
        #self.waitAndCall(0.1*60, self.setup)
        #self.waitAndCall(0.1*60, self.enableProtocol)
        #self.waitAndCall(10 *60, self.disableProtocol)
        #self.waitAndCall(0.1 *60, self.stop)
        self.waitAndCall(0.5*60, self.setup)         
        self.waitAndCall(15 *60, self.stop)     
       
    def setup(self):
        for host, connection in self.clientIterator():
            if self.hostid[host]:
                self.sendToConnection(connection, "addr " + str(self.hostid[host]))        
        self.sendToAll("clocksynce beacon interval 5000 5000")
        self.sendToAll("clocksynce beacon on")
        self.sendToAll("clocksynce heartbeat on")  
    
    def postHook(self):         
        self.runner.testbed.archiveLogs("local-clocksynce-gtimer")
        
    def preHook(self):
        devlist = os.listdir("/dev/")        
        regex = re.compile('^ttyUSB')        
        self.portList = sorted([port for port in devlist if regex.match(port)])
        address = 1
        for port in self.portList:
            self.hostid[port] = address
            address += 1   

testbed = LocalTestbed(serverHost, serverPort, flasher, hexFilePath
                       ,pyterm, logFilePath)
testbed.flashNodes()
experiment = ExperimentRunner(VtimerExperiment, testbed) 
experiment.run()
        
