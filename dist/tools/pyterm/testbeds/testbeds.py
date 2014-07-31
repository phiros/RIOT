#!/usr/bin/python2
# -*- coding: utf-8 -*-

import os, re, datetime
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
    
    def defaultArchivePostfix(self, experimentName = None):
        if not experimentName:
            experimentName = "unknown"
        time = datetime.datetime.now().strftime("%Y-%m-%d_%H_%M_%S")
        postfix = "-" + experimentName +"_" + time  
        return postfix     
    
    def printAndCall(self, cmdString):
        print(cmdString) 
        call(cmdString, shell=True)         
    
    
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
        self.printAndCall("parallel-ssh -h %s -l %s 'python %s'" % (self.hostFile, self.userName, self.flasher))
        
    def cleanLogs(self):        
        self.printAndCall("rm -rf %s/*.log" % (self.logFilePath))
        
    def archiveLogs(self, postfix = None): 
        postfix = self.defaultArchivePostfix(postfix)
        logDir = self.logFilePath.split("/")[-1]
        self.printAndCall("cd %s/..; tar -cjf archived_logs%s.tar.bz2 %s/*.log" % (self.logFilePath, postfix, logDir))
        
    def start(self):        
        self.printAndCall("parallel-ssh -h %s -l %s 'screen -S pyterm -d -m python %s'" % (self.hostFile, self.userName, self.pyterm))
        
    def stop(self):        
        self.printAndCall("parallel-ssh -h %s -l %s 'screen -X -S pyterm quit'" % (self.hostFile, self.userName))
        
class LocalTestbed(Testbed):
    
    def __init__(self, serverHost = None, serverPort=None, flasher = None, hexfilePath = None, pyterm = None, logFilePath = None):
        self.serverHost = serverHost
        self.serverPort = str(serverPort)
        self.flasher = flasher
        self.hexFilePath = hexfilePath
        self.pyterm = pyterm
        self.logFilePath = logFilePath
        
    def findPorts(self):
        devlist = os.listdir("/dev/")        
        regex = re.compile('^ttyUSB')        
        return sorted([port for port in devlist if regex.match(port)])

    def flashNodes(self):       
        self.printAndCall("python %s %s" % (self.flasher, self.hexFilePath))
        
    def cleanLogs(self):      
        self.printAndCall("rm -rf %s/*.log" % (self.logFilePath))
        
    def archiveLogs(self, postfix = None): 
        postfix = self.defaultArchivePostfix(postfix)
        logDir = self.logFilePath.split("/")[-1]
        self.printAndCall("cd %s/..; tar -cjf archived_logs%s.tar.bz2 %s/*.log" % (self.logFilePath, postfix, logDir))
        
    def start(self):
        portList = self.findPorts()
        for port in portList:           
            self.printAndCall("screen -S pyterm-%s -d -m python %s -H %s -rn %s -p /dev/%s" % (port, self.pyterm, port, port, port))
            
    def stop(self):
        portList = self.findPorts()
        for port in portList:         
            self.printAndCall("screen -X -S pyterm-%s quit" % (port))
            
        