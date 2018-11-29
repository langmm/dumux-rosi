#
# compares the dumux solution of 1d root model to its analytical solution
#
# D. Leitner, 2018
#

import os
import matplotlib.pyplot as plt
from vtk_tools import *
from math import *
import van_genuchten as vg

# go to the right place
path = os.path.dirname(os.path.realpath(__file__))
os.chdir(path)
os.chdir("../../../build-cmake/rosi_benchmarking/rootsystem")

# run dumux
os.system("./rootsystem input/b2.input")
p_, z_ = read3D_vtp_data("benchmark2-00001.vtp", True)
h_ = vg.pa2head(p_)
plt.plot(h_, z_[1:, 2], "r+")  # cell data
plt.ylabel("Depth (m)")
plt.xlabel("Xylem pressure (cm)")
plt.show()

