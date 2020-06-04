import sys
sys.path.append("../../../build-cmake/rosi_benchmarking/python_solver/")

from solver.xylem_flux import XylemFluxPython  # Python hybrid solver
import solver.plantbox as pb
import solver.rsml_reader as rsml
from rosi_richards import RichardsSP  # C++ part (Dumux binding)
from rosi_richards_cyl import RichardsCylFoam  # C++ part (Dumux binding)
from solver.richards import RichardsWrapper  # Python part

import solver.van_genuchten as vg
from solver.fv_grid import *
import solver.richards_solver as rich

from math import *
import numpy as np
import matplotlib.pyplot as plt
import timeit
from multiprocessing import Pool


def sinusoidal(t):
    """ sinusoidal function (used for transpiration) """
    return np.sin(2. * pi * np.array(t) - 0.5 * pi) + 1.

""" 
Benchmark M1.2 static root system in soil, coupled to cylindrical richards

also works parallel with mpiexec (only slightly faster, due to overhead)
"""

""" Parameters """
min_b = [-4., -4., -15.]
max_b = [4., 4., 0.]
domain_volume = 8 * 8 * 15  # cm3
cell_number = [10, 10, 15]
loam = [0.08, 0.43, 0.04, 1.6, 50]
initial = -659.8

kx = 4.32e-2 
kr = 1.728e-4
trans = 6.4  # cm3 /day (sinusoidal)
wilting_point = -15000  # cm

NC = 10  # NC-1 are dof of the cylindrical problem
logbase = 1.5

periodic = False  
 
sim_time = 3  # [day]
NT = 216  # iteration ( 3 days / 216 = 1200 s)

""" Initialize macroscopic soil model """
cpp_base = RichardsSP()
s = RichardsWrapper(cpp_base)
s.initialize()
s.createGrid(min_b, max_b, cell_number)  # [cm]
s.setHomogeneousIC(initial - 7.5, True)  # cm pressure head, equilibrium
s.setTopBC("noFlux")
s.setBotBC("noFlux")
s.setVGParameters([loam])
s.initializeProblem()
s.ddt = 1.e-5  # [day] initial Dumux time step 

""" Initialize xylem model (a)"""
old_rs = XylemFluxPython("../grids/RootSystem_big.rsml")
ana = pb.SegmentAnalyser(old_rs.rs)
ana.filter("creationTime", 0., 8.)
ana.crop(pb.SDF_PlantBox(7., 7., 14.))  # that's akward.. (but I wait for the final rsml).
ana.pack()
print("\nAfter 8 days", len(ana.segments))
rs = pb.MappedSegments(ana.nodes, ana.segments, ana.data["radius"]) 
r = XylemFluxPython(rs)  # <-- or final of you root system
r.rs.setRectangularGrid(pb.Vector3d(min_b[0], min_b[1], min_b[2]), pb.Vector3d(max_b[0], max_b[1], max_b[2]),
                        pb.Vector3d(cell_number[0], cell_number[1], cell_number[2]))  
r.setKr([kr])  # [cm^3/day]
r.setKx([kx])  # [1/day]
picker = lambda x, y, z : s.pick([x, y, z])
r.rs.setSoilGrid(picker)  # maps segments

nodes = r.get_nodes()
segs = r.get_segments()
cci = picker(nodes[0, 0], nodes[0, 1], nodes[0, 2])  # collar cell index

print("Cut to", len(segs), "segments\n")

# for i in r.rs.cell2seg[cci]:
#     print("cell fun...", i)
# input()

inner_radii = np.array(r.rs.radii)
outer_radii = r.segOuterRadii()
seg_length = r.segLength()

""" Initialize local soil models (around each root segment) """
ns = len(seg_length)  # number of segments 
cyls = [None] * ns
ndof = NC - 1

print("start initializing")


def initialize_cyl(i): 
    """ initialization of  local cylindrical model """
    a_in = inner_radii[i]
    a_out = outer_radii[i]  
    if a_in < a_out: 
        z = 0.5 * (nodes[segs[i, 0], 2] + nodes[segs[i, 1], 2])       
        points = np.logspace(np.log(a_in) / np.log(logbase), np.log(a_out) / np.log(logbase), NC, base=logbase)
        grid = FV_Grid1Dcyl(points)
        richards = rich.FV_Richards(grid, loam)  
        richards.h0 = np.ones((ndof,)) * (initial + z + 7.5)        
        return richards  
    else:
        print("Segment", i, "[", a_in, a_out, "]")  # this happens if elements are not within the domain
        return []


def simulate_cyl(cyl):
    try:
        cyl.solve([sim_time / NT], sim_time / NT / 10, False)         
    except:
        x = cyl.grid.mid
        y = cyl.h0
        plt.plot(x, y)
        plt.xlabel("x (cm)")
        plt.ylabel("pressure (cm)")
        plt.show()                        
    return cyl  


pool = Pool()  # defaults to number of available CPU's
start_time = timeit.default_timer()
cyls = pool.map(initialize_cyl, range(ns))         
print ("Initialized in", timeit.default_timer() - start_time, " s")

""" Simulation """
print("Starting simulation")
start_time = timeit.default_timer()

rsx = np.zeros((ns,))  # xylem pressure at the root soil interface
dt = sim_time / NT 

min_rx, min_rsx, collar_sx, collar_flux = [], [], [], []  # cm
water_uptake, water_collar_cell, water_cyl, water_domain = [], [], [], []  # cm3

rsx = np.zeros((ns,))  # matric potential at the root soil interface [cm]
cell_volumes = s.getCellVolumes()
inital_soil_water = np.sum(np.multiply(np.array(s.getWaterContent()), cell_volumes))

net_flux = np.zeros(cell_volumes.shape)
realized_inner_fluxes = np.zeros((len(cyls),))

for i in range(0, NT):

    t = i * dt

    """ 
    Xylem model 
    """
    csx = s.getSolutionHeadAt(cci)  # [cm]         
    for j, rc in enumerate(cyls):  # for each segment
        rsx[j] = rc.getInnerHead()  # [cm]            
                        
    soil_k = np.divide(vg.hydraulic_conductivity(rsx, cyls[0].soil), inner_radii)  # only valid for homogenous soil      
    print("Conductivities", np.min(soil_k), kr)                                                                                                          
    rx = r.solve(t, -trans * sinusoidal(t) , csx, rsx, False, wilting_point, soil_k)  # [cm]   * sinusoidal(t)
    collar_flux.append(r.collar_flux(t, rx, rsx, soil_k))

    min_rsx.append(np.min(np.array(rsx)))
    collar_sx.append(csx)
    min_rx.append(np.min(np.array(rx)))                
    print("Minimum of cylindrical model {:g} cm, minimal root xylem pressure {:g} cm".format(min_rsx[-1], min_rx[-1])) 
                                        
    """
    Local soil model
    """                  
    # proposed_inner_fluxes = r.segFluxes(0., rx, rsx, approx=False)  # [cm3/day]                 
    proposed_outer_fluxes = r.splitSoilFluxes(net_flux / dt)  
    
    for j, cyl in enumerate(cyls):  # boundary condtions
        l = seg_length[j]    
        rx_approx = 0.5 * (rx[segs[j][0]] + rx[segs[j][1]])
        cyl.bc[(0, 0)] = ("rootsystem", [rx_approx, kr])  
        # cyl.bc[(0, 0)] = ("rootsystem_exact", [rx[segs[j][0]], rx[segs[j][1]], kr, kx, inner_radii[j], l ])  
        dx_outer = cyl.grid.nodes[ndof] - cyl.grid.mid[ndof - 1]
        q_outer = proposed_outer_fluxes[j] / (2 * np.pi * outer_radii[j] * l)
        cyl.bc[(ndof - 1, 1)] = ("flux_in", [q_outer , 0., dx_outer]) 
                                
    cyls = pool.map(simulate_cyl, cyls)  # simulate
    
    for j, cyl in enumerate(cyls):  # res          
        realized_inner_fluxes[j] = cyl.getInnerFlux() * (2 * np.pi * inner_radii[j] * seg_length[j]) / dt   
    
    """
    Macroscopic soil model
    """   
    soil_water = np.multiply(np.array(s.getWaterContent()), cell_volumes)  # water per cell [cm3]
    soil_fluxes = r.sumSoilFluxes(realized_inner_fluxes)  # [cm3/day]  
    s.setSource(soil_fluxes.copy())  # [cm3/day], richards.py
    
    summed_soil_fluxes = 0.
    for k, v in soil_fluxes.items():
        summed_soil_fluxes += v
        
    print("Fluxes: realized per segment", summed_soil_fluxes, np.sum(realized_inner_fluxes), "predescribed: ", collar_flux[-1], -trans * sinusoidal(t))
    
    s.solve(dt)   
    
    new_soil_water = np.multiply(np.array(s.getWaterContent()), cell_volumes)
    net_flux = new_soil_water - soil_water  # change in water per cell [cm3] 
    for k, root_flux in soil_fluxes.items():
        net_flux[k] -= root_flux * dt    
    print("Summed net flux {:g}, max movement {:g} cm3".format(np.sum(net_flux), np.max(net_flux)))  # summed fluxes should equal zero
    soil_water = new_soil_water
    
    """ 
    Water (for output)
    """        
    water_domain.append(np.sum(soil_water))  # from previous time step 
    water_collar_cell.append(soil_water[cci])
    sum_flux = 0.
    for k, f in soil_fluxes.items():
        sum_flux += f
    water_uptake.append(sum_flux)  # cm3/day  
    
    cyl_water = 0.    
    for k in r.rs.cell2seg[cci]:
        cyl_water_content = cyls[j].getWaterContent()  # segment 0
        for j, wc in enumerate(cyl_water_content):
            r1 = cyls[k].grid.nodes[j]
            r2 = cyls[k].grid.nodes[j + 1]
            cyl_water += np.pi * (r2 * r2 - r1 * r1) * seg_length[k] * wc        
    # print("Water volume cylindric", cyl_water, "soil", soil_water[cci])
    water_cyl.append(cyl_water)

    n = round(float(i) / float(NT) * 100.)
    print("[" + ''.join(["*"]) * n + ''.join([" "]) * (100 - n) + "], {:g} days".format(s.simTime))
    
print ("Coupled benchmark solved in ", timeit.default_timer() - start_time, " s")

fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2)
 
ax1.set_title("Water amount")
ax1.plot(np.linspace(0, sim_time, NT), np.array(water_collar_cell), label="water cell")
ax1.plot(np.linspace(0, sim_time, NT), np.array(water_cyl), label="water cylindric")
ax1.legend()
ax1.set_xlabel("Time (days)")
ax1.set_ylabel("(cm3)")
 
ax2.set_title("Pressure")
ax2.plot(np.linspace(0, sim_time, NT), np.array(collar_sx), label="soil at root collar")
ax2.plot(np.linspace(0, sim_time, NT), np.array(min_rx), label="root collar")
ax2.plot(np.linspace(0, sim_time, NT), np.array(min_rsx), label="1d model at root surface")
ax2.legend()
ax2.set_xlabel("Time (days)")
ax2.set_ylabel("Matric potential (cm)")
# plt.ylim(-15000, 0) 
  
ax3.set_title("Water uptake")
ax3.plot(np.linspace(0, sim_time, NT), -np.array(water_uptake))
ax3.set_xlabel("Time (days)")
ax3.set_ylabel("Uptake (cm/day)") 
  
ax4.set_title("Water in domain")
ax4.plot(np.linspace(0, sim_time, NT), np.array(water_domain))
ax4.set_xlabel("Time (days)")
ax4.set_ylabel("cm3")
plt.show()   

fig, ax1 = plt.subplots()
x_ = np.linspace(0, sim_time, NT)
ax1.plot(x_, trans * sinusoidal(x_), 'k', label="potential")  # potential transpiration * sinusoidal(x_)
print(sim_time / NT)
ax1.plot(x_, -np.array(water_uptake), 'g', label="actual")  # actual transpiration (neumann)
ax1.plot(x_, -np.array(collar_flux), 'r:', label="collar flux")  # actual transpiration (neumann)
ax2 = ax1.twinx()
ax2.plot(np.linspace(0, sim_time, NT), np.array(min_rx), label="root collar")
ax1.set_xlabel("Time [d]")
ax1.set_ylabel("Transpiration $[cm^3 d^{-1}]$")
ax2.set_ylabel("Min collar pressure $[cm]$")
fig.legend()
plt.show()

