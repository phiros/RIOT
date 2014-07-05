#!/usr/bin/python2
# -*- coding: utf-8 -*-

from subprocess import call

class Testbed():
    def __init__(self):
        pass
    
    def initCleanWithFlash(self):
        self.stop()
        self.cleanLogs()
        self.flashNodes()
        self.start()
        
    def initClean(self):
        self.stop()
        self.cleanLogs()
        self.start()
    
    def flashNodes(self):
        raise NotImplementedError("Inherit from Testbed and implement flashNodes") 
    
    def cleanLogs(self):
        raise NotImplementedError("Inherit from Testbed and implement flashNodes") 
    
    def archiveLogs(self, experiment = None):
        raise NotImplementedError("Inherit from Testbed and implement flashNodes") 
    
    def start(self):
        raise NotImplementedError("Inherit from Testbed and implement flashNodes")  
    
    def stop(self):
        raise NotImplementedError("Inherit from Testbed and implement flashNodes")   
    
    
class DESTestbed(Testbed):
    
    def __init__(self, serverHost = None, serverPort=None, userName = None, flasher = None, 
                 hexfilePath = None, pyterm = None, logFilePath = None, hostFile = None):
        self.serverHost = serverHost
        self.serverPort = str(serverPort)
        self.userName = userName
        self.flasher = flasher
        self.hexFilePath = hexfilePath
        self.pyterm = pyterm
        self.logFilePath = logFilePath
        self.hostFile = hostFile
        
    def flashNodes(self):
        call("parallel-ssh -h %s -l %s 'python %s'" % (self.hostFile, self.userName, self.flasher))
        
    def cleanLogs(self):
        call("rm -rf %s/*.log" % (self.logFilePath))
        
    def archiveLogs(self, experiment = None):
        time = datetime.datetime.now().strftime("%Y-%m-%d_%H_%M_%S")
        call("tar -cjf %s/archived_logs_%s_%s.tar.bz2 %s/*.log" % (self.logFilePath, experiment, time, self.logFilePath))
        
    def start(self):
        call("parallel-ssh -h %s -l %s 'screen -S pyterm -d -m python %s'" % (self.hostFile, self.userName, self.pyterm))
        
    def stop(self):
        call("parallel-ssh -h %s -l %s 'screen -X -S pyterm quit'" % (self.hostFile, self.userName))
          