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
        print("flashNodes: parallel-ssh -h %s -l %s 'python %s'" % (self.hostFile, self.userName, self.flasher))
        call("parallel-ssh -h %s -l %s 'python %s'" % (self.hostFile, self.userName, self.flasher), shell = True)
        
    def cleanLogs(self):
        print("cleanLogs: rm -rf %s/*.log" % (self.logFilePath))
        call("rm -rf %s/*.log" % (self.logFilePath), shell = True)
        
    def archiveLogs(self, experiment = None):
        time = datetime.datetime.now().strftime("%Y-%m-%d_%H_%M_%S")
        call("tar -cjf %s/../archived_logs_%s_%s.tar.bz2 %s/*.log" % (self.logFilePath, experiment, time, self.logFilePath), shell = True)
        
    def start(self):
        print("start: parallel-ssh -h %s -l %s 'screen -S pyterm -d -m python %s'" % (self.hostFile, self.userName, self.pyterm))
        call("parallel-ssh -h %s -l %s 'screen -S pyterm -d -m python %s'" % (self.hostFile, self.userName, self.pyterm), shell = True)
        
    def stop(self):
        print("stop: parallel-ssh -h %s -l %s 'screen -X -S pyterm quit'" % (self.hostFile, self.userName))
        call("parallel-ssh -h %s -l %s 'screen -X -S pyterm quit'" % (self.hostFile, self.userName), shell = True)
        
class LocalTestbed(Testbed):
    
    def __init__(self, serverHost = None, serverPort=None, flasher = None, hexfilePath = None, pyterm = None, logFilePath = None):
        self.serverHost = serverHost
        self.serverPort = str(serverPort)
        self.flasher = flasher
        self.hexFilePath = hexfilePath
        self.pyterm = pyterm
        self.logFilePath = logFilePath
        
    def findPorts():
        devlist = os.listdir("/dev/")        
        regex = re.compile('^ttyUSB')        
        return sorted([port for port in devlist if regex.match(port)])

    def flashNodes(self):
        print("flashNodes: python %s %s'" % (self.flasher, self.hexFilePath))
        call("flashNodes: python %s %s'" % (self.flasher, self.hexFilePath), shell = True)
        
    def cleanLogs(self):
        print("cleanLogs: rm -rf %s/*.log" % (self.logFilePath))
        call("rm -rf %s/*.log" % (self.logFilePath), shell = True)
        
    def archiveLogs(self, experiment = None):
        time = datetime.datetime.now().strftime("%Y-%m-%d_%H_%M_%S")
        call("tar -cjf %s/../archived_logs_%s_%s.tar.bz2 %s/*.log" % (self.logFilePath, experiment, time, self.logFilePath), shell = True)
        
    def start(self):
        portList = findPorts()
        for port in portList:
            print("screen -S pyterm-%s -d -m python %s -H %s" % (port, self.pyterm, port))
            call("screen -S pyterm-%s -d -m python %s -H %s" % (port, self.pyterm, port), shell = True)
            
    def stop(self):
        for port in portList:
            print("screen -X -S pyterm-%s quit" % (port))
            call("screen -X -S pyterm-%s quit" % (port), shell = True)
            
        