#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab, getopt
import time, calendar, collections

from loganalyzer import ClocksyncEvalLogAnalyzer



def print_help():
    supportedProtocols = ["gtsp", "ftsp", "pulsesync"]
    print("offsetCalibration.py -p <protocol> -l <path-to-log-files>")
    print("supported protocols: ")
    for p in supportedProtocols:
        print("\t" + p)

def main(argv):
   protocol = None
   logdir = None
   
   try:
      opts, args = getopt.getopt(argv,"hp:l:",["protocol=","log-dir="])
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
   if not protocol or not logdir:
       print_help()
       sys.exit(1)
    
   loga = ClocksyncEvalLogAnalyzer(logdir, ".*" + protocol +  " on.*", ".*" + protocol +  " off.*")
   loga.analyze() 
   callculatedOffset = loga.getCalibrationOffset()
   # substract 1 percent
   returnedOffset = int(callculatedOffset - callculatedOffset/100)  
   print("Calibration offset: "+ str(loga.getCalibrationOffset()))

if __name__ == "__main__":
   main(sys.argv[1:])               
                 
