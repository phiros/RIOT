#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab, getopt
import time, calendar, collections

from loganalyzer import ClocksyncEvalLogAnalyzer

def print_help():
    supportedProtocols = ["gtsp", "ftsp", "pulsesync"]
    print("plotSyncError.py -p <protocol> -l <path-to-log-files> [-G|-L]")
    print("-G: global error")
    print("-L: local error")
    print("-y: y max")
    print("-s: show only sync phase")
    print("supported protocols: ")
    for p in supportedProtocols:
        print("\t" + p)

def main(argv):
   protocol = None
   logdir = None
   globalError = False
   yrange = 0
   synconly = False
   
   try:
      opts, args = getopt.getopt(argv,"hp:l:GLy:s",["protocol=","log-dir=", "Global", "Local", "y-range=", "sync-only"])
   except getopt.GetoptError:
          print_help()
          sys.exit(2)
   for opt, arg in opts:
       if opt == '-h':
           print_help()
           sys.exit()
       elif opt in ("-p", "--protocol"):
           protocol = arg
       elif opt in ("-l", "--log-dir"):
           logdir = arg
       elif opt in ("-G", "--Global"):
           globalError = True  
       elif opt in ("-L", "--Local"):
           globalError = False  
       elif opt in ("-y", "--y-range"):
           yrange = int(arg)
       elif opt in ("-s", "--sync-only"):
           synconly = True                            
        
   if not protocol or not logdir:
       print_help()
       sys.exit(1)
   
   if synconly:
       loga = ClocksyncEvalLogAnalyzer(logdir, ".*" + protocol +  " on.*", ".*" + protocol +  " off.*")
   else:
       loga = ClocksyncEvalLogAnalyzer(logdir, ".*RIOT.*", "$a")
    
   loga.analyze() 
   if globalError:
        xvals = loga.maxGlobalError.keys()
        yvals = loga.maxGlobalError.values()
        pylab.title("global sync error")
   else:      
       xvals = loga.localErrorMaxAvg.keys()
       yvals = loga.localErrorMaxAvg.values()
       pylab.title("local sync error")
   pylab.plot(xvals, yvals)
   if yrange>0:
       pylab.ylim([0,yrange])
   # pylab.ylim([0,300])
   pylab.show()

if __name__ == "__main__":
   main(sys.argv[1:])               
