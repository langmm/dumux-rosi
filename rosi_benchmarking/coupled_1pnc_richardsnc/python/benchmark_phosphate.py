''' Run Phosphate Benchmark '''

import os
import matplotlib.pyplot as plt
from vtk_tools import *
import van_genuchten as vg
import math

# go to the right place
path = os.path.dirname(os.path.realpath(__file__))
os.chdir(path)
os.chdir("../../../build-cmake/rosi_benchmarking/coupled_1pnc_richardsnc")

# trans = 1.e-7 * ((2 * 0.02 * math.pi) * 1) * 1.e-6 * 1000. * (24.*3600.)  # cm/s -> kg/day
# print("Transpiration", trans, "kg/day") # in the input file

# Plot Comsol solution
# comsol = np.loadtxt("python/gradients.txt", skiprows = 9)
# r_ = comsol[:, 0]
# c_ = comsol[:, 15]
# plt.plot(r_, c_, "g")

# plt.ylabel("g/cm⁴3")
# plt.show()

os.system("./coupled_1p2c input/benchmark_phosphate.input")

c_, y_ = read3D_vtp_data_line("benchmark_phosphate2-00000.vtu", 1e-3, 13)
p_, y_ = read3D_vtp_data_line("benchmark_phosphate2-00000.vtu", 1e-3, 2)
h1_ = vg.pa2head(p_)
print(y_.shape)
print("h", np.min(h1_), np.max(h1_))
print("y", np.min(y_), np.max(y_))
print("c", np.min(c_), np.max(c_))

plt.plot(y_, h1_, "r")
plt.ylabel("$\psi$ cm pressure head")
# plt.plot(y_ * 100, c_ * 10, "r")
# plt.ylabel("$c$ ???")
plt.xlabel("cm")
plt.show()
