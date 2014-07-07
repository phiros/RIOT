#!/usr/bin/env python
'''
Created on Jan 9, 2013

@author: stephan
'''
import os
import subprocess
import re
import threading
import time
from os.path import expanduser

class Flash():
    '''
    This is a module which does nothing more than flashing nodes with a hex-file
    '''
    

    def __init__(self):
        '''
        This constructor creates a list of all /dev/ttyUSBx files in the system
        which is then used to flash, if there isn't a specific list of devices
        given by the user.
        '''
        self.portlist = findports()
    
    def doflash(self, hexfile, portlist=None):
        '''
        This method is flashing all found nodes given with the portlist, or, in
        case the portlist was not given, all nodes found in /dev/
        '''
        
        if portlist is None:
            portlist = self.portlist
        
        curdir = os.getcwd()
        os.chdir(expanduser("~"))
        
        flashthreads = []
        print '\n'
        for port in portlist:
            print 'Flashing node /dev/' + port
            current = FlashThread(hexfile, port)
            flashthreads.append(current)
            current.start()
        
        for thread in flashthreads:
            thread.join()

        for thread in flashthreads:
            thread.showstatus()            
        os.chdir(curdir)

class FlashThread(threading.Thread):
    
    def __init__(self, hexfile, port):
        threading.Thread.__init__(self)
        self.hexfile = hexfile
        self.port = port
        self.status = 0
    
    def run(self):
        result = 0
        
        while result == 0:
            print self.port + ': opening subprocess for lpc2k_pgm'
            process = subprocess.Popen(['lpc2k_pgm', '/dev/' + self.port , self.hexfile], stdout=subprocess.PIPE)
            
            #1 minute should be ample time to finish
            time_passed = 0
            while time_passed < 60:
                if process.poll() is None :
                    time.sleep(5)
                    time_passed += 5
                else :
                    break
            
            if process.poll() is None :
                process.terminate()
                break
                
            result = self.checkresult(process.stdout.read())

    def checkresult(self, output=None):
            
            if output is None:
                output = 'was none'
            
            syncErrorString = re.compile('^Download Canceled: Unexpected response to sync,')
            syncSucString = re.compile('^Reset CPU \(into user code\)')
            result = -1
            
            soutarray = output.split('\n') 
            
            for line in soutarray :
                if syncErrorString.match(line) :
                    result = 0  
                elif syncSucString.match(line) :
                    self.status = 1
                    result = 1
            return result        
    
    def showstatus(self):
        if self.status == 0:
            print self.port + ": Flashing unsuccessful, please manually reconnect the node."
        elif self.status == 1:        
            print self.port + ": Flashing successful."
            
                
                
def findports():
        '''
        This method finds all devices in the system which match the pattern
        ^ttyUSB in directory /dev/
        '''
        devlist = os.listdir("/dev/")
        
        regex = re.compile('^ttyUSB')
        
        return sorted([port for port in devlist if regex.match(port)])
        
if __name__ == "__main__":
    
    flash = Flash()
    if len(flash.portlist) > 0 :
        print "Flashing the following nodes:"
        for node in flash.portlist :
            print "/dev/" + node
        if len(sys.argv)>1:
            flash.doflash(sys.argv[1])
            print "\nFlash completed."
            return 0
        else:
            print "usage: " + sys.argv[0] + " pathToHexFile"      
    else :
        print "No nodes to flash found in directory '/dev/'"
