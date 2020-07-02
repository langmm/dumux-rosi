import sys
sys.path.append("../../../build-cmake/rosi_benchmarking/python_solver/")

import numpy as np
from scipy import sparse
import scipy.sparse.linalg as LA
import matplotlib.pyplot as plt

import solver.van_genuchten as vg
from solver.fv_grid import *
import solver.richards_solver as rich

import matplotlib.pyplot as plt
import numpy as np
import os

from mpi4py import MPI
comm = MPI.COMM_WORLD
rank = comm.Get_rank()

import time

""" 
Cylindrical 1D model 

"""

ndof = 200  # ndof = 1000, dann passts

nodes = np.linspace(0.02, 0.6, ndof + 1)
grid = FV_Grid1Dcyl(nodes)

loam = [0.045, 0.43, 0.04, 1.6, 50]

sim_times = [10, 20]
# richards = rich.FV_Richards(grid, loam)  # general solver using UMFPack
richards = rich.FV_Richards1D(grid, loam)  # specialized 1d solver (direct banded is sufficient)
richards.h0 = np.ones((ndof,)) * (-100)
richards.bc[(0, 0)] = ["flux_out", [-0.1, -15000, 0.02 ]]

t = time.time()
h = richards.solve(sim_times, 0.2)
print("elapsed time", time.time() - t)

col = ["r*", "g*", "b", "m", "c", "y"] * 5
for i in range(0, len(sim_times)):
    plt.plot(richards.grid.centers(), h[i, :], col[i], label = "Time {:g} days".format(sim_times[i]))
plt.xlabel("cm")
plt.ylabel("matric potential (cm)")
plt.legend()

os.chdir("../../../build-cmake/rosi_benchmarking/soil_richards/python")
data = np.loadtxt("cylinder_1d_Comsol.txt", skiprows = 8)
z_comsol = data[:, 0]
plt.plot(z_comsol + 0.02, data[:, 25], "k", label = "comsol 10 days")
plt.plot(z_comsol + 0.02, data[:, -1], "k:", label = "comsol 20 days")

plt.xlabel("distance from root axis (cm)")
plt.xlabel("soil matric potential (cm)")
plt.legend()
plt.show()

# soil = vg.Parameters(loam)
# water_volume = []
# for h_ in h:
#     water_volume.append(np.sum(np.multiply(vg.water_content(h_, soil), grid.volume)))
# volume = np.sum(grid.volume)
# water_volume = np.array(water_volume)
# v = np.pi * (0.6 ** 2 - 0.02 ** 2) * vg.water_content(-100, soil)
#
# print("\n")
# print("Volume {:g} == {:g} cm3".format(volume, np.pi * (0.6 ** 2 - 0.02 ** 2)))
# print("Water in domain (first time step): actual {:g} analytical {:g}".format(water_volume[0], v - (0.1 * (2 * 0.02 * np.pi)) * sim_times[0]))
#
# # plt.plot(sim_times, water_volume)
# # plt.xlabel("time (day)")
# # plt.ylabel("water_volume (cm3)")
# # plt.plot([sim_times[0], sim_times[-1]], [v, v])
# # plt.show()

