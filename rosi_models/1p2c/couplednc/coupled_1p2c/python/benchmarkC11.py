''' Run Benchmakrk C11 '''

import os
import matplotlib.pyplot as plt
from vtk_tools import *
import van_genuchten as vg
import math

# go to the right place
path = os.path.dirname(os.path.realpath(__file__))
os.chdir(path)
os.chdir("../../../build-cmake/rosi_benchmarking/coupled")

soiltype = 1  # sand, loam, clay
q_root = 0.1  # cm/day
soils = ["Sand", "Loam", "Clay"]

trans = q_root * (2 * 0.02 * math.pi) * 1 * 1.e-6 * 1000.  # cm/day -> kg/day

# run simulation
os.system("./coupled input/benchmarkC11.input -Soil.Layer.Number {} -RootSystem.Collar.Transpiration {}"
          .format(soiltype + 1, trans))

# Figure 1
leg_str = [];
for i in range(0, 14):
    print("benchmarkC11-000{0:02d}.vtu".format(i))
    p_, y_ = read3D_vtp_data_line("benchmarkC11-000{0:02d}.vtu".format(i), False)
    h1_ = vg.pa2head(p_)
    plt.plot(np.array(y_) * 100, h1_)
    leg_str.append("{}".format(i * 2) + " days")

plt.legend(leg_str)
plt.ylabel('$\psi$ (cm)')
plt.xlabel('y axis (cm)')
plt.title(soils[soiltype] + ", q_{root}=" + str(q_root) + "d")
plt.show()

# Read results
with open("benchmarkC11_actual_transpiration.txt", 'r') as f:
    d = np.loadtxt(f, delimiter = ',')

c = (24 * 3600)  #  kg/s -> cm^3 / per day # 1.e-3 * 1.e6 / (2 * 0.02 * math.pi) * 1
t = d[:, 0] / (24 * 3600)

fig, ax1 = plt.subplots()

# 0 time, 1 actual transpiration, 2 potential transpiration, 3 maximal transpiration, 4 collar pressure, 5 calculated actual transpiration
ax1.plot(t, d[:, 2] * c, 'k')  # potential transpiration
ax1.plot(t, d[:, 1] * c, 'r-,')  # reference, actual transpiration
# ax1.plot(t, d[:, 3] * c, 'r:')  # reference, maximal transpiration

ax1.legend(['Pot trans', 'actual trans'])
# ax1.axis((0, t[-1], 0, 12))
ax1.set_xlabel("Time $[d]$")
ax1.set_ylabel("Transpiration rate $[mm \ d^{-1}]$")

plt.show()
