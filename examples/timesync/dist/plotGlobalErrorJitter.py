#!/usr/bin/env python2
import pylab

from loganalyzer import ClocksyncEvalLogAnalyzer

logdir = "/home/philipp/.pyterm/log"
protocol = "gtsp"
loga = ClocksyncEvalLogAnalyzer(logdir, ".*RIOT.*", "$a")
loga.analyze()

xvals = []
yvals = []
localvals = []
i = 0
for key in loga.heartBeatJitterDict.keys():
    if i>3:
        break
    for tupple in loga.heartBeatJitterDict[key]:
        xvals.append(tupple[0])
        yvals.append(tupple[0]- tupple[3])
        localvals.append(tupple[2])
        print "xval: " + str(tupple[0]) + " yval: " + str(tupple[1]) + " localTime " + str(tupple[2])
    heady = yvals[0]
    headx = xvals[0]
    yvals = map(lambda x: x - yvals[0], yvals)
    xvals = map(lambda x: x - headx, xvals)
    deltalocal = localvals[-1] - localvals[0]
    deltax = xvals[-1] - xvals[0]
    print " local:  " + str(deltalocal)
    print " server: " + str(deltax)
    print " diff: " + str(deltax - deltalocal)
    pylab.plot(xvals, yvals)
    xvals = []
    yvals = []
    i += 1

pylab.title("global sync error jitter")
pylab.show() 