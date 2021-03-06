# cylinder problem (in 1D)
#
# compares the dumux solution with the comsol solution
#
# D. Leitner, 2018
#

import os
import matplotlib.pyplot as plt
from vtk_tools import *
import van_genuchten as vg

# go to the right place
path = os.path.dirname(os.path.realpath(__file__))
os.chdir(path)
os.chdir("../../../build-cmake/rosi_benchmarking/soil_richards")

# run dumux
os.system("./richards1d_cyl input/cylinder_1d.input")

# plot dumux results
s_, p_, z_ = read1D_vtp_data("cylinder_1d-00003.vtp")
h_ = vg.pa2head(p_)
plt.plot((z_ - 0.0002) * 100, h_, "b",)

# read and plot comsol data
os.chdir("../../../build-cmake/rosi_benchmarking/soil_richards/python")
data = np.loadtxt("cylinder_1d_Comsol.txt", skiprows=8)
z_comsol = data[:, 0]
h_comsol = data[:, -1]
plt.plot(z_comsol, h_comsol, "r")

# plt.plot(z_,h_,  "b")
plt.xlabel('distance from the root surface (cm)')
plt.ylabel('pressure head (cm)')
plt.legend(["dumux", "comsol"], loc='lower right')
plt.show()

