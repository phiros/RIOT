#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab, getopt
import time, calendar, collections

from loganalyzer import ClocksyncEvalLogAnalyzer

loga = None

def print_help():
  supportedProtocols = ["gtsp", "ftsp", "pulsesync"]
  print("plotSyncError.py [--sync-only -p <protocol>] [-G|-L] <path-to-log-files>")
  print("-G: global error")
  print("-L: local error")
  print("-y: y max")
  print("-s: show only sync phase")
  print("supported protocols: ")
  for p in supportedProtocols:
    print("\t" + p)

def main(argv):
  protocol = None
  globalError = False
  yrange = 0
  synconly = False
  out = False

  try:
    opts, logdirs = getopt.getopt(argv,"o:hp:GLy:s",["output=", "protocol=", "Global", "Local", "y-range=", "sync-only"])
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

  if globalError:
    pylab.title("global sync error")
    for loga in logas:
      xvals = pylab.linspace(loga.minBackboneTime, loga.maxBackboneTime, 100)
      yvals = global_diff_wrapper(loga, xvals)
      pylab.plot(xvals, yvals, label = loga.name)
      # errors = global_diff_wrapper(loga, xvals)
      # pylab.errorbar(xvals, yvals, errors)
  else:
    pylab.title("local sync error")
    for loga in logas:
      xvals = loga.localErrorMaxAvg.keys()
      yvals = loga.localErrorMaxAvg.values()
      pylab.plot(xvals, yvals, label = loga.name)
      # errors = [0 for i in xvals]
      # pylab.errorbar(xvals, yvals, errors)

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

