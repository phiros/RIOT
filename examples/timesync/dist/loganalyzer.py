#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab
import time, calendar, collections

class ClocksyncEvalLogAnalyzer():
    def __init__(self, logdir = "", beginRe = ".*", endRe = ".*"):
        self.triggerDict = dict()
        self.heartbeatDict = dict()
        self.localErrorMaxAvg = dict()
        self.logdir = logdir
        self.maxServerTime = 0
        self.beginRe = beginRe
        self.endRe = endRe
        self.heartBeatBucketSize = 10 * 1000 * 1000 # 10 s
        self.maxGlobalError = dict()
        self.globalMinDiff = sys.maxint
        
    def analyze(self):
        for root, dirs, files in os.walk(self.logdir):
            for file in files:
                if file.endswith(".log"):
                    fileName = os.path.join(root, file)
                    self.fillLocalErrorDict(fileName)
                    self.fillHeartbeatDict(fileName)
        self.triggerToMaxLocalError()
        self.heartBeatToGlobalError()
        od = collections.OrderedDict(sorted(self.localErrorMaxAvg.items()))
        self.localErrorMaxAvg = od
        self.scaleGlobalErrorTime()
        od = collections.OrderedDict(sorted(self.maxGlobalError.items()))
        self.maxGlobalError = od
                    
    def getCalibrationOffset(self):
        calCount = self.avgLocalErrorToCalibration()   
        od = collections.OrderedDict(sorted(calCount.items())) 
        calCount = od  
        calOffset = self.calibrationOffset() 
        return calOffset      

    def dateTimeToTimeStamp(self, serverDate, serverTime):
        tempString = serverDate + " " + serverTime
        t = tempString.split(",")
        dateTimePart = t[0]
        millisecondPart = float("0." + t[1])
        
        timetuple = time.strptime(tempString.split(",")[0], "%Y-%m-%d %H:%M:%S")
        unixtime = float(calendar.timegm(timetuple))
        return (unixtime + millisecondPart)
    
    def fillHeartbeatDict(self, fileName):
        bucketsize = self.heartBeatBucketSize # 10 seconds buckets
        logpats = r'(\S+)\s+(\S+).*\#eh, a: (\S+), gl: (\S+), gg: (\S+),.*'
        hostpats = r'.*/(\S+)\.log'
        endPats = r'' + self.endRe
        beginPats = r'' + self.beginRe
        logpat = re.compile(logpats)        
        hostpat = re.compile(hostpats)        
        beginpat = re.compile(beginPats)        
        endpat = re.compile(endPats)
        continueToRead = False
        with open(fileName) as f:
            hostMatch = hostpat.match(fileName)
            if hostMatch:
                hostName = hostMatch.groups()[0]
            for line in f:
                if endpat.match(line):
                    break
                if not continueToRead:
                    if beginpat.match(line):
                        continueToRead = True
                    continue
                
                match = logpat.match(line)
                if(match): 
                    tuple = match.groups()
                    serverDate = tuple[0]
                    serverTime = tuple[1]
                    serverTimeStamp = self.dateTimeToTimeStamp(serverDate, serverTime)*1000*1000
                    sourceId = int(tuple[2])
                    localTime = int(tuple[3])
                    globalTime = int(tuple[4])
                    globalServerDiff = serverTimeStamp - globalTime
                    if globalServerDiff < self.globalMinDiff:
                        self.globalMinDiff = globalServerDiff
                    timeBucket = int(math.floor(serverTimeStamp/bucketsize)*bucketsize)                   
                    if timeBucket > self.maxServerTime:
                        self.maxServerTime = timeBucket
                        
                    tupple = (serverTimeStamp, globalServerDiff, sourceId, localTime, globalTime)
                    if self.heartbeatDict.has_key(timeBucket):
                        self.heartbeatDict[timeBucket].append(tupple)                                       
                    else:
                        self.heartbeatDict[timeBucket] = [tupple]                                             
                         
                        
    def heartBeatToGlobalError(self):
        self.globalMinBucket = sys.maxint
        self.localMaxBucket = 0
        for bucket, tuppleList in self.heartbeatDict.items():
            if bucket < self.globalMinBucket:
                self.globalMinBucket = bucket
            if bucket > self.localMaxBucket:
                self.localMaxBucket = bucket
                
            minDiff = sys.maxint
            maxDiff = 0
            for tupple in tuppleList:
                globalServerDiff = tupple[1]
                if globalServerDiff < minDiff:
                    minDiff = globalServerDiff
                if globalServerDiff > maxDiff:
                    maxDiff = globalServerDiff
            self.maxGlobalError[bucket] = maxDiff - minDiff
      
        
    def scaleGlobalErrorTime(self):
        newGlobalError = dict()
        for time in self.maxGlobalError.keys():
            timeInSeconds = int((time-self.globalMinBucket)/(1000*1000))            
            newGlobalError[timeInSeconds] =  self.maxGlobalError[time]
        self.maxGlobalError = newGlobalError      
                   
    

    def fillLocalErrorDict(self, fileName):
        logpats = r'(\S+)\s+(\S+).*\#et, a: (\S+), c: (\S+), tl: (\S+), tg: (\S+)'
        hostpats = r'.*/(\S+)\.log'
        endPats = r'' + self.endRe
        beginPats = r'' + self.beginRe
        logpat = re.compile(logpats)        
        hostpat = re.compile(hostpats)        
        beginpat = re.compile(beginPats)        
        endpat = re.compile(endPats)
        
        continueToRead = False
        with open(fileName) as f:
            hostMatch = hostpat.match(fileName)
            if hostMatch:
                hostName = hostMatch.groups()[0]
            for line in f:
                if endpat.match(line):
                    break
                if not continueToRead:
                    if beginpat.match(line):
                        continueToRead = True
                    continue
                
                match = logpat.match(line)
                if(match):                    
                    tuple = match.groups()
                    serverDate = tuple[0]
                    serverTime = tuple[1]
                    serverTimeStamp = self.dateTimeToTimeStamp(serverDate, serverTime)
                    beaconSender = int(tuple[2])
                    beaconCounter = int(tuple[3])
                    localTime = int(tuple[4])
                    globalTime = int(tuple[5])
                    if not self.triggerDict.has_key(beaconSender):
                        self.triggerDict[beaconSender] = dict()
                        if not self.triggerDict[beaconSender].has_key(beaconCounter):
                            self.triggerDict[beaconSender][beaconCounter] = []
                    else:
                        if not self.triggerDict[beaconSender].has_key(beaconCounter):
                            self.triggerDict[beaconSender][beaconCounter] = []                          
                    self.triggerDict[beaconSender][beaconCounter].append((hostName, serverTimeStamp, localTime, globalTime)) 

    def triggerToMaxLocalError(self):
        bucketsize = 10 # -> 0.1 ms buckets
        for address in self.triggerDict.iterkeys():        
            for count in self.triggerDict[address].iterkeys():
                countList = self.triggerDict[address][count]
                serverTimeSum = 0
                entryCount = 0
                maxTime = 0
                minTime = sys.maxint
                for tupple in countList:  
                    #if not (tupple[0] == "ttyUSB0" or tupple[0] == "ttyUSB2"):
                    #    continue               
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
                if not self.localErrorMaxAvg.has_key(timeBucket):
                    self.localErrorMaxAvg[timeBucket] = maxDiff
                else:
                    if self.localErrorMaxAvg[timeBucket]<maxDiff:
                        self.localErrorMaxAvg[timeBucket] = maxDiff
 
    def avgLocalErrorToCalibration(self):
        bucketSize = 10 # 10 us buckets
        self.offsetCount = dict()
        maxBucket = 0    
        for bucket, error in self.localErrorMaxAvg.items():
            timeBucket = error - (error % bucketSize)
            if timeBucket>maxBucket:
                maxBucket = timeBucket
            if self.offsetCount.has_key(timeBucket):
                self.offsetCount[timeBucket] += 1
            else:
                self.offsetCount[timeBucket] = 1
        for bucket in range(0, maxBucket + bucketSize, bucketSize):
            if not self.offsetCount.has_key(bucket):
                self.offsetCount[bucket] = 0
        return self.offsetCount 

    def calibrationOffset(self): 
        returnBucket = 0
        maxCount = 0
        for bucket, count in self.offsetCount.items():
            if count>maxCount:
                maxCount = count
                returnBucket = bucket
        return returnBucket
