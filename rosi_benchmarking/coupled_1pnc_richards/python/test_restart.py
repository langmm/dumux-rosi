''' singleroot '''

import os
import matplotlib.pyplot as plt
from vtk_tools import *
import van_genuchten as vg
import math
import numpy as np

name = "test_restart2"  # this name should be unique

# go to the right place
path = os.path.dirname(os.path.realpath(__file__))
os.chdir(path)
os.chdir("../../../build-cmake/rosi_benchmarking/coupled_1pnc_richards")

# # run simulation
os.system("./coupled_periodic_1pnc_richards input/" + name + ".input")

# move results to folder 'name'
if not os.path.exists("results_" + name):
    os.mkdir("results_" + name)
os.system("mv " + name + "* " + "results_" + name + "/")
os.system("cp input/" + name + ".input " + "results_" + name + "/")

#      * 0 time [s], 1 actual transpiration [kg/s], 2 potential transpiration [kg/s], 3 maximal transpiration [kg/s],
#      * 4 collar pressure [Pa], 5 calculated actual transpiration [cm^3/day], 6 simtime [s], 7 hormone leaf mass [kg],
#      * 8 hormone collar flow rate [kg/s], 9 hormone root system mass [kg] , 10 hormone source rate [kg/s]
with open("results_" + name + "/" + name + "_actual_transpiration.txt", 'r') as f:
    d = np.loadtxt(f, delimiter = ',')

c = 24 * 3600  # s / day
t = d[:, 0] / c  # [s] -> [day]

""" Plot hormone rate and mass """
fig, ax1 = plt.subplots()

ax1.plot(d[:, 0] / c, 1000 * d[:, 8] * c, "r")
ax1.tick_params(axis = 'y', labelcolor = "r")
ax1.set_ylabel("leaves (g/day)", color = "r")
ax1b = ax1.twinx()
ax1b.plot(d[:, 0] / c, 1000 * d[:, 10] * c, "b")
ax1b.tick_params(axis = 'y', labelcolor = "b")
ax1b.set_ylabel("root system (g/day)", color = "b")
ax1.set_xlabel("time (days)")
ax1.set_title("Hormone production rate")
plt.savefig("results_" + name + "/" + name + '_productionrate.pdf', dpi=300)

fig, ax2 = plt.subplots()

ax2.plot(d[:, 0] / c, 1000 * d[:, 7], "r")
ax2.tick_params(axis = 'y', labelcolor = "r")
ax2.set_ylabel("leaves (g)", color = "r")
ax2b = ax2.twinx()
ax2b.plot(d[:, 0] / c, 1000 * d[:, 9], "b")
ax2b.tick_params(axis = 'y', labelcolor = "b")
ax2b.set_ylabel("root system (g)", color = "b")
ax2.set_xlabel("time (days)")
ax2.set_title("Hormone mass")
plt.savefig("results_" + name + "/" + name + '_hormoneMass.pdf', dpi=300)

""" Plot transpiration """
fig, ax3 = plt.subplots()
ax3.plot(t, 1000 * d[:, 2] * c, 'k')  # potential transpiration
ax3.plot(t, 1000 * d[:, 1] * c, 'r-,')  # actual transpiration
ax3b = ax3.twinx()
ctrans = np.cumsum(np.multiply(1000 * d[1:, 1] * c, (t[1:] - t[:-1])))
np.savetxt('results_' + name + "/" + name + '_ctrans.txt', ctrans, delimiter = '\n')   # save cumulative transpiration file
ax3b.plot(t[1:], ctrans, 'c--', color = 'blue')  # cumulative transpiration (neumann)
ax3b.tick_params(axis= 'y', labelcolor = 'b')
ax3.set_xlabel("time (days)")
ax3.set_ylabel("transpiration (g/day)")
ax3b.set_ylabel("Cumulative transpiration $[g]$", color = "b")
ax3.legend(["potential", "actual", "cumulative"], loc = 'upper left')
plt.savefig("results_" + name + "/" + name + '_transpiration.pdf', dpi=300)

""" Plot root collar pressure """
fig, ax4 = plt.subplots()
ax4.plot(d[:, 0] / c, (d[:, 4] - 1.e5) * 100. / 1.e3 / 9.81, 'r-')  # root colalr pressure head (convert from Pa to Head)  
ax4.set_xlabel("time (days)")
ax4.set_ylabel("Pressure at root collar (cm)")
plt.savefig("results_" + name + "/" + name + '_collarPressure.pdf', dpi=300)

plt.show()
