#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import os, re, math, sys, pylab
import time, calendar, collections
import numpy as np

class ClocksyncEvalLogAnalyzer():
    def __init__(self, logdir, beginRe = ".*", endRe = ".*"):
        self.name = logdir
        self.firstTime = False
        # Diconary: (sendingNode, receivingNode) -> Rate of transfered signals
        self.adjDict = dict()
        self.heartBeatByIdDict = dict()
        self.maxAdj = 0.0
        # Diconary: ReceivingNode -> SendingNode (Int) -> TriggerId (Int) -> (Server Time, Global Time, Local Time)
        self.triggerDict = dict()
        self.backVsGlobal = dict()
        self.heartbeatDict = dict()
        self.localErrorMaxAvg = dict()
        self.localError = dict()
        self.localErrorAvg = dict()
        self.logdir = logdir
        self.minBackboneTime = sys.maxint
        self.maxBackboneTime = 0
        self.maxServerTime = 0
        self.beginRe = beginRe
        self.endRe = endRe
        self.heartBeatBucketSize = 10 * 1000 * 1000 # 10 s
        self.maxGlobalError = dict()
        self.globalMinDiff = sys.maxint
        self.hosts = self.loadHostFile()
        self.globalErrorFitFunctionsById = dict()
        self.analyze()

    # Returns a dictornary NodeId (Int) -> NodeName (String)
    def loadHostFile(self):
        hosts = dict()
        id = 1
        with open("hosts") as f:
            for line in f:
                hosts[id] = line.strip()
                id += 1
        return hosts

    def analyze(self):
        for root, dirs, files in os.walk(self.logdir):
            for file in files:
                if file.endswith(".log"):
                    fileName = os.path.join(root, file)
                    self.analyzeTriggerEvents(fileName)
                    self.fillHeartbeatDict(fileName)
                    self.fillHeartBeatByIdDict(fileName)

        # Fix offset and find the count of sended trigger per node:
        self.sendedTrigger = 0
        for rec in self.triggerDict:
            for sender in self.triggerDict[rec]:
              if len(self.triggerDict[rec][sender]) > self.sendedTrigger:
                  self.sendedTrigger = len(self.triggerDict[rec][sender])
              for trigger in self.triggerDict[rec][sender]:
                  server, glob, local = self.triggerDict[rec][sender][trigger]
                  self.triggerDict[rec][sender][trigger] = server - self.firstTime, glob, local

        self.triggerToMaxLocalError()
        self.heartBeatToGlobalError()
        self.globalServerDiffToFunctionFit()
        od = collections.OrderedDict(sorted(self.localErrorMaxAvg.items()))
        self.localErrorMaxAvg = od
        self.scaleGlobalErrorTime()
        od = collections.OrderedDict(sorted(self.localErrorAvg.items()))
        self.localErrorAvg = od
        od = collections.OrderedDict(sorted(self.maxGlobalError.items()))
        self.maxGlobalError = od
        od = collections.OrderedDict(sorted(self.heartbeatDict.items()))
        self.heartbeatDict = od

        # Normalize the adjDict
        for relation in self.adjDict.keys():
            self.adjDict[relation] /= self.maxAdj

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
        endPats = r'' + self.endRe
        beginPats = r'' + self.beginRe
        logpat = re.compile(logpats)
        beginpat = re.compile(beginPats)
        endpat = re.compile(endPats)
        continueToRead = False
        with open(fileName) as f:
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

                    if serverTimeStamp > self.maxBackboneTime:
                        self.maxBackboneTime = serverTimeStamp
                    if serverTimeStamp < self.minBackboneTime:
                        self.minBackboneTime = serverTimeStamp

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
                    self.backVsGlobal[serverTimeStamp] = globalTime

    def fillHeartBeatByIdDict(self, fileName):
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
                    tupple = (serverTimeStamp, globalServerDiff, localTime, globalTime)
                    if self.heartBeatByIdDict.has_key(sourceId):
                        self.heartBeatByIdDict[sourceId].append(tupple)
                    else:
                        self.heartBeatByIdDict[sourceId] = [tupple]

    def globalServerDiffToFunctionFit(self):
        for id, tuppleList in self.heartBeatByIdDict.items():
            xvals = []
            yvals = []
            for tupple in tuppleList:
                serverTime = tupple[0]
                globalServerDiff = tupple[1]
                xvals.append(serverTime)
                yvals.append(globalServerDiff)
            fit, residual, _, _, _ =  np.polyfit(xvals, yvals, 1, full = True) # assume linear relationship
            self.globalErrorFitFunctionsById[id] = (np.poly1d(fit), residual)


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

    # Analyze all trigger events in fileName and fills
    # the triggerDict and the adjDict with the results.
    def analyzeTriggerEvents(self, fileName):
        logpats = r'(\S+)\s+(\S+).*\#et, a: (\S+), c: (\S+), tl: (\S+), tg: (\S+)'
        hostpats = r'.*/(\S+)\.log'
        endPats = r'' + self.endRe
        beginPats = r'' + self.beginRe
        logpat = re.compile(logpats)
        hostpat = re.compile(hostpats)
        beginpat = re.compile(beginPats)
        endpat = re.compile(endPats)

        hostMatch = hostpat.match(fileName)
        if hostMatch:
            hostName = hostMatch.groups()[0]

        continueToRead = False
        with open(fileName) as file:
            for line in file:
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

                    try:
                        beaconCounter = int(tuple[3])
                        localTime = int(tuple[4])
                        globalTime = int(tuple[5])
                        if not self.firstTime or self.firstTime > serverTimeStamp:
                            self.firstTime = serverTimeStamp

                        if not self.triggerDict.has_key(hostName):
                            self.triggerDict[hostName] = dict()
                        if not self.triggerDict[hostName].has_key(beaconSender):
                            self.triggerDict[hostName][beaconSender] = dict()
                        self.triggerDict[hostName][beaconSender][beaconCounter] = serverTimeStamp, globalTime, localTime

                        # Updating adj dict
                        if not self.hosts.has_key(beaconSender):
                          print "Found an alien: " + str(beaconSender)
                          continue

                        relation = self.hosts[beaconSender], hostName
                        if not self.adjDict.has_key(relation):
                            self.adjDict[relation] = 1.0
                        else:
                            self.adjDict[relation] += 1.0

                        if self.adjDict[relation] > self.maxAdj:
                            self.maxAdj = self.adjDict[relation]
                    except Exception, e:
                        print "Multithread: " + line



    def triggerToMaxLocalError(self):
        bucketsize = 10 # -> 0.1 ms buckets
        for (recv1, recv2) in self.adjDict.iterkeys():
            # Inspect every connection only once
            if recv1 < recv2:
                continue
            # ignore all nodes with a weaker connection than 80%
            if self.adjDict[recv1, recv2] < 0.8 or self.adjDict.get((recv2, recv1), 0) < 0.8:
                continue
            if not self.triggerDict.has_key(recv1):
                print "No trigger for " + recv1 + " found"
                continue

            if not self.triggerDict.has_key(recv2):
                print "No trigger for " + recv2 + " found"
                continue

            commonTriggers = set(self.triggerDict[recv1].keys()).intersection(set(self.triggerDict[recv2].keys()))

            # self.localError[recv1, recv2] = dict()

            trigger = 0
            for commonTrigger in commonTriggers:
                # Dictionaries: Trigger ID -> Event
                events1 = self.triggerDict[recv1][commonTrigger]
                events2 = self.triggerDict[recv2][commonTrigger]

                for id in set(events1.iterkeys()).intersection(set(events2.iterkeys())):
                    trigger += 1
                    server1, global1, local1 = events1[id]
                    server2, global2, local2 = events2[id]
                    meanServer = (server1 + server2) / 2
                    timeBucket = math.floor(meanServer/bucketsize) * bucketsize
                    error = abs(global1 - global2)
                    if self.localErrorMaxAvg.get(timeBucket, -1000) < error:
                        self.localErrorMaxAvg[timeBucket] = error

                    summe, count = self.localErrorAvg.get(timeBucket, (0, 0))
                    self.localErrorAvg[timeBucket] = summe + error, count + 1

                    self.localError[meanServer] = error

            if trigger < (self.sendedTrigger / 2):
                print recv1 + ", " + recv2 + ": common trigger only covers " + str(trigger * 100 / self.sendedTrigger) + "%"

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
