import vtk
import numpy as np
import matplotlib.pyplot as plt

#
# vtk_tools
#
# D. Leitner, 2018
#


#
# opens a vtp and returns the vtk polydata class
#
def read_polydata(name):
    reader = vtk.vtkXMLPolyDataReader()
    reader.SetFileName(name)
    reader.Update()
    polydata = reader.GetOutput()
    return polydata


#
# converts a vtp to dgf
#
def vtp2dgf(name):
    pd = read_polydata(name + ".vtp")  # read vtp

    file = open(name + ".dgf", "w")  # write dgf
    file.write("DGF\n")
    # vertex
    file.write('Vertex\n')
    Np = pd.GetNumberOfPoints()
    points = pd.GetPoints()
    pdata = pd.GetPointData()
    Npd = pdata.GetNumberOfArrays()
    file.write('parameters {:g}\n'.format(Npd))
    for i in range(0, Np):
        p = np.zeros(3,)
        points.GetPoint(i, p)
        file.write('{:g} {:g} {:g} '.format(p[0], p[1], p[2]))
        for j in range(0, Npd):  # write point data - todo lets pick ids
            pdataj = pdata.GetArray(j)
            d = pdataj.GetTuple(i)
            file.write('{:g} '.format(d[0]))
        file.write('\n')

    file.write('#\n');
    file.write('Simplex\n');
    # cells
    Nc = pd.GetNumberOfCells()
    cdata = pd.GetCellData()
    Ncd = cdata.GetNumberOfArrays()
    file.write('parameters 2\n'.format(Ncd))
    for i in range(0, Nc - 1):
        cpi = vtk.vtkIdList()
        pd.GetCellPoints(i, cpi)
        for j in range(0, cpi.GetNumberOfIds()):  # write cell ids
            file.write('{:g} '.format(cpi.GetId(j)))
        for j in range(0, Ncd):  # write cell data - todo lets pick ids
            cdataj = cdata.GetArray(j)
            d = cdataj.GetTuple(i)
            file.write('{:g} '.format(d[0]))
        file.write('\n')
    # i dont know how to use these in dumux
    file.write('#\n')
    file.write('BOUNDARYSEGMENTS\n')  # how do i get the boundary segments into DUMUX ?
    file.write('2 0\n')
    file.write('3 {:g}\n'.format(Np - 1))  # vertex id, but index starts with 0
    file.write('#\n');
    file.write('BOUNDARYDOMAIN\n')
    file.write('default 1\n');
    file.write('#\n')

    print("finished writing " + name + ".dgf")
    file.close()


#
# returns the cell or vertex data (index 0 and 2 hard coded)) of vtp file
#
def read1D_vtp_data(name):
    polydata = read_polydata(name)

    data = polydata.GetPointData()  # for box method
    nocd = data.GetNumberOfArrays()
    p = data.GetArray(0)  # pressure
    try:
        noa = p.GetNumberOfTuples()
    except:
        data = polydata.GetCellData()  # for CCTpfa
        nocd = data.GetNumberOfArrays()
        p = data.GetArray(0)  # pressure
        noa = p.GetNumberOfTuples()

    p_ = np.ones(noa,)
    for i in range(0, noa):
        d = p.GetTuple(i)
        p_[i] = d[0]

    return p_


#
# returns the cell or vertex data (index 0) of vtp file
#
def read3D_vtp_data(name):
    polydata = read_polydata(name)

    try:  # Box
        data = polydata.GetPointData()
        nocd = data.GetNumberOfArrays()
        p = data.GetArray(0)  # pressure
        noa = p.GetNumberOfTuples()
        cc = False
    except:  # CCTpfa
        data = polydata.GetCellData()
        nocd = data.GetNumberOfArrays()
        p = data.GetArray(0)  # pressure
        noa = p.GetNumberOfTuples()
        cc = True

    p_ = np.ones(noa,)
    for i in range(0, noa):
        d = p.GetTuple(i)
        p_[i] = d[0]

    points = polydata.GetPoints()

    if cc:
        Nc = polydata.GetNumberOfLines()  # GetPointId (int ptId)
        z_ = np.zeros((Nc, 3))
        for i in range(0, Nc):
            c = polydata.GetCell(i)
            ids = c.GetPointIds()
            p1 = np.zeros(3,)
            points.GetPoint(ids.GetId(0), p1)
            p2 = np.zeros(3,)
            points.GetPoint(ids.GetId(1), p2)
            z_[i, :] = 0.5 * (p1 + p2)
        return p_, z_
    else:  # not cell
        Np = polydata.GetNumberOfPoints()
        z_ = np.zeros((Np, 3))
        for i in range(0, Np):
            p = np.zeros(3,)
            points.GetPoint(i, p)
            z_[i, :] = p
        return p_, z_


if __name__ == "__main__":
    # manually set absolute path
    path = "/home/daniel/workspace/DUMUX/dumux-rosi/build-cmake/rosi_benchmarking/richards1d/"
    vtp2dgf(path + "jan1a-00000")

