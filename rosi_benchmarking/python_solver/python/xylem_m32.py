import sys
sys.path.append("../../../build-cmake/rosi_benchmarking/python_solver/")

from solver.xylem_flux import XylemFluxPython  # Python hybrid solver
import solver.plantbox as pb
import solver.rsml_reader as rsml

from math import *
import numpy as np
from numpy.linalg.linalg import norm
from scipy import sparse
import scipy.sparse.linalg as LA

import matplotlib.pyplot as plt

from mpi4py import MPI
comm = MPI.COMM_WORLD
rank = comm.Get_rank()

""" 
Benchmark M3.2 Root system: steady state small root system solved with the Python/cpp Hybrid solver
(does not work in parallel)
"""

""" Parameters """
L = 50  # length of single straight root [cm]
a = 0.2  # radius [cm] <--------------------------------------------------------- ???
kz = 4.32e-2  # [cm^3/day]
kr = 1.728e-4  # [1/day]
p_s = -200  # static soil pressure [cm]
p0 = -500  # dircichlet bc at top
simtime = 14  # [day] for task b

""" Read and rsml"""
polylines, props, funcs = rsml.read_rsml("../grids/RootSystem.rsml")
nodes, segs = rsml.get_segments(polylines, props)
radii, seg_ct, types = rsml.get_parameter(polylines, funcs, props)
nodes = np.array(nodes)  # for slicing in the plots
nodes2 = []  # Conversions...
for n in nodes:
    nodes2.append(pb.Vector3d(n[0] / 10., n[1] / 10., n[2] / 10.))  # [mm] -> [cm], and convert to a list of Vetor3d
segs2 = []
nodeCTs = np.zeros((len(nodes), 1))  # we need node creation times
for i, s in enumerate(segs):
    nodeCTs[s[1]] = seg_ct[i]
    segs2.append(pb.Vector2i(int(s[0]), int(s[1])))
radii = np.array(radii) / 10.  # [mm]->[cm]
types = np.array(types, dtype = np.int64) - 1  # index must start with 0

rs = pb.MappedSegments(nodes2, nodeCTs, segs2, radii, types)

""" Numerical solution (a) """

r = XylemFluxPython(rs)
r.setKr([kr])
r.setKx([kz])

rx_hom = r.solve(0., p0 - p_s, False)
rx = r.getSolution(rx_hom, [p_s])

fig, (ax1, ax2) = plt.subplots(1, 2)
ax1.plot(rx, nodes[:, 2] , "r*")
ax1.set_xlabel("Xylem pressure (cm)")
ax1.set_ylabel("Depth (m)")

print()

""" Numerical solution (b) """

kx0 = np.array([[0, 6.74e-02], [2, 7.48e-02], [4, 8.30e-02], [6, 9.21e-02], [8, 1.02e-01], [10, 1.13e-01], [12, 1.26e-01], [14, 1.40e-01], [16, 1.55e-01], [18, 1.72e-01], [20, 1.91e-01], [22, 2.12e-01], [24, 2.35e-01], [26, 2.61e-01], [28, 2.90e-01], [30, 3.21e-01], [32, 3.57e-01]])
kx1 = np.array([[0, 4.07e-04], [1, 5.00e-04], [2, 6.15e-04], [3, 7.56e-04], [4, 9.30e-04], [5, 1.14e-03], [6, 1.41e-03], [7, 1.73e-03], [8, 2.12e-03], [9, 2.61e-03], [10, 3.21e-03], [11, 3.95e-03], [12, 4.86e-03], [13, 5.97e-03], [14, 7.34e-03], [15, 9.03e-03], [16, 1.11e-02], [17, 1.36e-02]])
kr0 = np.array([[0, 1.14e-03], [2, 1.09e-03], [4, 1.03e-03], [6, 9.83e-04], [8, 9.35e-04], [10, 8.90e-04], [12, 8.47e-04], [14, 8.06e-04], [16, 7.67e-04], [18, 7.30e-04], [20, 6.95e-04], [22, 6.62e-04], [24, 6.30e-04], [26, 5.99e-04], [28, 5.70e-04], [30, 5.43e-04], [32, 5.17e-04]])
kr1 = np.array([[0, 4.11e-03], [1, 3.89e-03], [2, 3.67e-03], [3, 3.47e-03], [4, 3.28e-03], [5, 3.10e-03], [6, 2.93e-03], [7, 2.77e-03], [8, 2.62e-03], [9, 2.48e-03], [10, 2.34e-03], [11, 2.21e-03], [12, 2.09e-03], [13, 1.98e-03], [14, 1.87e-03], [15, 1.77e-03], [16, 1.67e-03], [17, 1.58e-03]])
r.setKrTables([kr0[:, 1], kr1[:, 1], kr1[:, 1]], [kr0[:, 0], kr1[:, 0], kr1[:, 0]])  # 3 types
r.setKxTables([kx0[:, 1], kx1[:, 1], kr1[:, 1]], [kx0[:, 0], kx1[:, 0], kr1[:, 0]])

rx_hom = r.solve(simtime, p0 - p_s, False)
rx = r.getSolution(rx_hom, [p_s])

ax2.plot(rx, nodes[:, 2] , "r*")
ax2.set_xlabel("Xylem pressure (cm)")
ax2.set_ylabel("Depth (m)")

plt.show()
