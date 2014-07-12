#!/usr/bin/env python2

import networkx as nx
import numpy as np
import string
import fileinput
import pygraphviz as pgv
import matplotlib.pyplot as plt

from loganalyzer import ClocksyncEvalLogAnalyzer

logdir = "/home/philipp/log"
protocol = "ftsp"

# G = nx.DiGraph()
gvd = pgv.AGraph(directed = True)
loga = ClocksyncEvalLogAnalyzer(logdir, ".*" + protocol +  " on.*", ".*" + protocol +  " off.*")
loga.analyze()

i = 0
for bucket in loga.heartbeatDict.keys():
    if i>2:
        break
    i += 1
    for tupple in loga.adjDict.keys():
        from_node = tupple[0]
        to_node = tupple[1]
        #print "from: " + from_node + " to: " + to_node + " val: "+ str(loga.adjDict[tupple])
        
        if float(loga.adjDict[tupple]) > 0.50:
            gvd.add_edges_from([(from_node, to_node)])
        # G.add_weighted_edges_from([(sline[0], sline[1], float(sline[2])*5)])
    
    # G = nx.to_agraph(G)
    
    # G.node_attr.update(color="red", style="filled")
    # G.edge_attr.update(color="black", width="2.0")
    
    # G.draw_random('/tmp/out.png', format='png', prog='neato')
    gvd.graph_attr.update(sep="+25,25", splines=True, overlap="scalexy", nodesep=0.6)
    gvd.node_attr.update(fontsize="10")
    
    gvd.layout(prog='neato')
    gvd.draw('/tmp/out' + str(i) + '.png' )