#!/usr/bin/env python2

import networkx as nx
import numpy as np
import string
import fileinput
import pygraphviz as pgv
import matplotlib.pyplot as plt

# G = nx.DiGraph()
gvd = pgv.AGraph(directed = True)
for tupple, val in fileinput.input():
    from_node = tupple[0]
    to_node = tupple[1]
    if float(val) > 0.50:
        gvd.add_edges_from([(from_node, to_node)])
    # G.add_weighted_edges_from([(sline[0], sline[1], float(sline[2])*5)])

# G = nx.to_agraph(G)

# G.node_attr.update(color="red", style="filled")
# G.edge_attr.update(color="black", width="2.0")

# G.draw_random('/tmp/out.png', format='png', prog='neato')
gvd.graph_attr.update(sep="+25,25", splines=True, overlap="scalexy", nodesep=0.6)
gvd.node_attr.update(fontsize="10")

gvd.layout(prog='neato')
gvd.draw('/tmp/out.png')