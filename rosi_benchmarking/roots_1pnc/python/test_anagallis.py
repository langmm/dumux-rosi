""" "Test root system part """
import os
import matplotlib.pyplot as plt
from vtk_tools import *
from math import *
import van_genuchten as vg
import threading
import time

# Go to the right place
path = os.path.dirname(os.path.realpath(__file__))
os.chdir(path)
os.chdir("../../../build-cmake/rosi_benchmarking/roots_1pnc")

# run dumux
os.system("./rootsystem_1pnc input/anagallis_stomata.input")

# 0 time [s], 1 actual transpiration [kg/s], 2 potential transpiration [kg/s], 3 maximal transpiration [kg/s],
# 4 collar pressure [Pa], 5 calculated actual transpiration [cm^3/day], 6 simtime [s], 7 hormone leaf mass [kg], 8 hormone flow rate [kg/s]
with open("anagallis_roots_actual_transpiration.txt", 'r') as f:
    d = np.loadtxt(f, delimiter = ',')

print()
c = 24 * 3600  #  [kg/s] -> [kg/per day]
print("potential", d[-1, 2] * c)
print("actual", d[-1, 1] * c)
print("actual", d[-1, 5] / 1000)

# Plot collar transpiration & pressure
fig, ax1 = plt.subplots()

c = 24 * 3600  #  [kg/s] -> [kg/per day]
t = d[:, 6] / (24 * 3600)  # [s] -> [day]

# 0 time, 1 actual transpiration, 2 potential transpiration, 3 maximal transpiration, 4 collar pressure, 5 calculated actual transpiration
ax1.plot(t, d[:, 8] * c, 'k')  # potential transpiration
# ax1.plot(t, d[:, 5] / 1000, 'r-,')  # actual transpiration (calculated)
# ax1.plot(t, d[:, 1] * c, 'g:')  # actual transpiration (neumann)

ax1.legend(['Potential', 'Actual', 'Actual'], loc = 'upper left')
# ax1.axis((0, t[-1], 0, 0.013))
ax1.set_xlabel("Time $[d]$")
ax1.set_ylabel("Transpiration rate $[kg \ d^{-1}]$")

plt.show()