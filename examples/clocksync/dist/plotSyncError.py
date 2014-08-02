#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab, getopt
import time, calendar, collections

from loganalyzer import ClocksyncEvalLogAnalyzer

colors = ["blue", "green", "red", "orange", "black"]

def print_help():
  supportedProtocols = ["gtsp", "ftsp", "pulsesync"]
  print "plotSyncError.py [-s -p <protocol>] [-G|-L] [-o output]<path-to-log-files>"
  print "-a: shows all measure points, not only the upper bound"
  print "-G: global error"
  print "-L: local error"
  print "-o file:"
  print "   doesn't open a window, but save the plot as image"
  print "   supported formats: jpg, pdf"
  print "-p protocol:"
  print "   set the protocol for the logs, only needed by -s"
  print "-s: show only sync phase"
  print "-y: y max"
  print("supported protocols: ")
  for p in supportedProtocols:
    print("\t" + p)

def main(argv):
  loga = None
  protocol = None
  globalError = False
  yrange = 0
  synconly = False
  out = False
  showAllPoints = False

  try:
    opts, logdirs = getopt.getopt(argv,"o:hp:GLy:sa",["output=", "protocol=", "Global", "Local", "y-range=", "sync-only", "all-points"])
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
    elif opt in ("-y", "--y-range"):
      yrange = int(arg)
    elif opt in ("-s", "--sync-only"):
      synconly = True
    elif opt in ("-o", "--output"):
      out = arg
    elif opt in ("-a", "--all-points"):
      showAllPoints = True

  if not logdirs:
    print("No log folders")
    print_help()
    sys.exit(2)

  if synconly:
    if not protocol:
      print "No protocol given"
      print_help()
      sys.exit(1)
    logas = [ClocksyncEvalLogAnalyzer(logdir, ".*" + protocol +  " on.*", ".*" + protocol +  " off.*") for logdir in logdirs]
  else:
    logas = [ClocksyncEvalLogAnalyzer(logdir, ".*RIOT.*", "$a") for logdir in logdirs]

  if len(logas) > len(colors):
    print "Warning: more log folders than colors defined"
    print "         plotting some graphs with same color"

  if globalError:
    pylab.title("global sync error")
    for loga in logas:
      # xvals = pylab.linspace(loga.minBackboneTime, loga.maxBackboneTime, 100)
      # yvals = global_diff_wrapper(loga, xvals)
      # pylab.plot(xvals, yvals, label = loga.name)
      pylab.plot(loga.backVsGlobal.keys(), loga.backVsGlobal.values(), "x", label = loga.name)
  else:
    pylab.title("local sync error")
    colorIndex = 0
    for loga in logas:
      color = colors[colorIndex % len(colors)]
      colorIndex += 1

      xvals = loga.localErrorMaxAvg.keys()
      yvals = loga.localErrorMaxAvg.values()
      pylab.plot(xvals, yvals, label = loga.name, color = color)

      if showAllPoints:
        pylab.plot(loga.localError.keys(), loga.localError.values(), "x", color = color)

  if yrange>0:
    pylab.ylim([0,yrange])

  pylab.xlabel("Reference time in $s$")
  pylab.ylabel("Error in $\mu s$")

  if len(logas) > 1:
    pylab.legend(loc='upper right')

  if out:
    pylab.savefig(out)
  else:
    pylab.show()

def identitiy(x):
  return x;

def global_diff_wrapper(loga, xvals):
  diffs = []
  for xval in xvals:
    diff, maxid, minid = global_get_max_diff(loga, xval)
    diffs.append(diff)
  return diffs

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

