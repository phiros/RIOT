#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab, getopt
import time, calendar, collections

from matplotlib.pyplot import *
from loganalyzer import ClocksyncEvalLogAnalyzer

loga = None

def print_help():
    supportedProtocols = ["gtsp", "ftsp", "pulsesync"]
    print "plotSyncError.py [--sync-only -p <protocol>] [-G|-L] [--] <path-to-log-files>* "
    print "-G plots global error"
    print "-L plots local error"
    print "-y y max"
    print "-x x max"
    print "-s show only sync phase, requires -p"
    print "-o output-path"
    print "   doens't open a window but saves the plot in output-path"
    print "   supported: PDF, PNG..."
    print "-p protocol"
    print "supported protocols: "
    for p in supportedProtocols:
        print("\t" + p)

def main(argv):
    protocol = None
    globalError = False
    ylim = 0
    xlim = 0
    synconly = False
    output = None
   
    try:
        opts, args = getopt.getopt(argv,"hp:l:GLy:x:s:o:",["protocol=","log-dir=", "Global", "Local", "y-limit=", "x-limit=", "sync-only", "output="])
    except getopt.GetoptError:
        print_help()
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print_help()
            sys.exit()
        elif opt in ("-p", "--protocol"):
            protocol = arg
        elif opt in ("-G", "--Global"):
            globalError = True  
        elif opt in ("-L", "--Local"):
            globalError = False  
        elif opt in ("-y", "--y-limit"):
            ylim = int(arg)
        elif opt in ("-x", "--x-limit"):
            xlim = int(arg)
        elif opt in ("-s", "--sync-only"):
            synconly = True
        elif opt in ("-o", "--output"):
            output = arg

    logdirs = args

    if not logdirs:
        print_help()
        sys.exit(2)

    if synconly:
        if not protocol:
            print "No protocol given"
            sys.exit(1)
        logas = [ClocksyncEvalLogAnalyzer(logdir, ".*" + protocol +  " on.*", ".*" + protocol +  " off.*") for logdir in logdirs]
    else:
        logas = [ClocksyncEvalLogAnalyzer(logdir, ".*RIOT.*", "$a") for logdir in logdirs]

    if globalError:
        pylab.title("global sync error")
        for loga in logas:
            xvals = loga.getMaxGlobalError().keys()
            yvals = loga.getMaxGlobalError().values()
            pylab.plot(xvals, yvals, label = loga.name)

    else:      
        pylab.title("local sync error")
        for loga in logas:
            xvals = loga.localErrorMaxAvg.keys()
            yvals = loga.localErrorMaxAvg.values()
            pylab.plot(xvals, yvals, label = loga.name)

    pylab.ylabel("sync error in $\mu s$")
    pylab.xlabel("experiment time in $s$")
    legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)

    if ylim>0:
        pylab.ylim([0, ylim])
    if xlim>0:
        pylab.xlim([0, xlim])

    if output:
        pylab.savefig(output)
    else:
        pylab.show()

def global_diff_wrapper(loga, xvals):
    diffs = []
    for xval in xvals:
        diff, maxid, minid = global_get_max_diff(loga, xval)
        diffs.append(diff)
    return diffs

def global_diff_error(loga, xvals):
    errors = []
    for xval in xvals:
        diff, maxid, minid = global_get_max_diff(loga, xval)
        maxIderror = loga.globalErrorFitFunctionsById[maxid][1][0]
        minIderror = loga.globalErrorFitFunctionsById[minid][1][0]
        #print str(maxIderror)
        if minIderror < maxIderror:
            errors.append(minIderror)
        else:
            errors.append(maxIderror)
    #print str(errors)
    return errors
  
def global_get_max_diff(loga, xval):
    minval = (sys.maxint, 0)
    maxval = (0, 0)
    for id, tupple in loga.globalErrorFitFunctionsById.items():
        fun = tupple[0]
        funval = fun(xval)
        if funval > maxval[0]:
            maxval = (funval, id)
        if funval < minval[0]:
            minval = (funval, id)
    return (maxval[0] - minval[0], maxval[1], minval[1])
        
        

if __name__ == "__main__":
    main(sys.argv[1:])               
