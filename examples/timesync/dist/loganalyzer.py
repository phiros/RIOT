#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab
import time, calendar, collections

logdir = "/home/philipp/log"

# map(sender, map(counter, List((node, serverTimeStamp, localTime, globalTime))))
triggerDict = dict()

localErrorMaxAvg = dict()

def dateTimeToTimeStamp(serverDate, serverTime):
    tempString = serverDate + " " + serverTime
    t = tempString.split(",")
    dateTimePart = t[0]
    millisecondPart = float("0." + t[1])
    
    timetuple = time.strptime(tempString.split(",")[0], "%Y-%m-%d %H:%M:%S")
    unixtime = float(calendar.timegm(timetuple))
    return (unixtime + millisecondPart)
    

def fillLocalErrorDict(fileName):
    logpats = r'(\S+)\s+(\S+).*\#et, a: (\S+), c: (\S+), tl: (\S+), tg: (\S+)'
    logpat = re.compile(logpats)
    hostpats = r'.*/(\S+)\.log'
    hostpat = re.compile(hostpats)
    with open(fileName) as f:
        hostMatch = hostpat.match(fileName)
        if hostMatch:
            hostName = hostMatch.groups()[0]
        for line in f:
            match = logpat.match(line)
            if(match):
                
                tuple = match.groups()
                serverDate = tuple[0]
                serverTime = tuple[1]
                serverTimeStamp = dateTimeToTimeStamp(serverDate, serverTime)
                beaconSender = int(tuple[2])
                beaconCounter = int(tuple[3])
                localTime = int(tuple[4])
                globalTime = int(tuple[5])
                if not triggerDict.has_key(beaconSender):
                    triggerDict[beaconSender] = dict()
                    if not triggerDict[beaconSender].has_key(beaconCounter):
                        triggerDict[beaconSender][beaconCounter] = []
                else:
                    if not triggerDict[beaconSender].has_key(beaconCounter):
                        triggerDict[beaconSender][beaconCounter] = []
                      
                #print("beaconSender: " + str(beaconSender) + " beaconCounter: " + str(beaconCounter))                      
                triggerDict[beaconSender][beaconCounter].append((hostName, serverTimeStamp, localTime, globalTime)) 


def triggerToAvgLocalError(triggerDict):
    bucketsize = 10 # -> 0.1 ms buckets
    for address in triggerDict.iterkeys():        
        for count in triggerDict[address].iterkeys():
            countList = triggerDict[address][count]
            serverTimeSum = 0
            entryCount = 0
            maxTime = 0
            minTime = sys.maxint
            for tupple in countList:
                entryCount += 1
                serverTimeSum += tupple[1]
                if tupple[3]>maxTime:
                    maxTime = tupple[3]
                if tupple[3]<minTime:
                    minTime = tupple[3]
            if serverTimeSum==0:
                continue
            maxDiff = maxTime - minTime
            meanServerTime = serverTimeSum/entryCount
            timeBucket = math.floor(meanServerTime/bucketsize)*bucketsize
            if not localErrorMaxAvg.has_key(timeBucket):
                localErrorMaxAvg[timeBucket] = maxDiff
            else:
                if localErrorMaxAvg[timeBucket]<maxDiff:
                    localErrorMaxAvg[timeBucket] = maxDiff
    
                 
                        
                        
i = 0
for root, dirs, files in os.walk(logdir):
    for file in files:
        if i<=2:
            i += 1
            if file.endswith(".log"):
                fileName = os.path.join(root, file)
                fillLocalErrorDict(fileName)
                triggerToAvgLocalError(triggerDict)
                od = collections.OrderedDict(sorted(localErrorMaxAvg.items()))
                localErrorMaxAvg = od
                
items = [(key, value) for key, value in localErrorMaxAvg.items() if value>0]
time, values = zip(*items)
pylab.title("max local error")
pylab.plot(time, values)
pylab.ylim([0, 5000])
pylab.show() 
                