// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   See the file COPYING for full copying permissions.                      *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 *****************************************************************************/
/*!
 * \file
 * \brief The spatial parameters class
 */
#ifndef DUMUX_ROOT_1P2C_SPATIALPARAMS_DGF_HH
#define DUMUX_ROOT_1P2C_SPATIALPARAMS_DGF_HH

#include <dune/common/exceptions.hh>
#include <dumux/common/math.hh>
#include <dumux/common/parameters.hh>
#include <dumux/material/spatialparams/fv1p.hh>
#include <dumux/material/components/simpleh2o.hh>

#include <dumux/io/inputfilefunction.hh> // in dumux-rosi

namespace Dumux {

/*!
 * \brief Root spatial parameters class for grid files.
 *
 * use initParameters to initialize the class with data from the grid file
 *
 */
template<class FVGridGeometry, class Scalar>
class RootOnePTwoCSpatialParamsDGF
    : public FVSpatialParamsOneP<FVGridGeometry, Scalar, RootOnePTwoCSpatialParamsDGF<FVGridGeometry, Scalar>> {
    using ThisType = RootOnePTwoCSpatialParamsDGF<FVGridGeometry, Scalar>;
    using ParentType = FVSpatialParamsOneP<FVGridGeometry, Scalar, ThisType>;
    using GridView = typename FVGridGeometry::GridView;
    using Element = typename GridView::template Codim<0>::Entity;
    using SubControlVolume = typename FVGridGeometry::SubControlVolume;
    using GlobalPosition = typename Element::Geometry::GlobalCoordinate;
    using Water = Components::SimpleH2O<Scalar>;

public:

    using PermeabilityType = Scalar; // export permeability type

    RootOnePTwoCSpatialParamsDGF(std::shared_ptr<const FVGridGeometry> fvGridGeometry):
        ParentType(fvGridGeometry) {
        // DGF specific (where is what)
        time0_ = Dumux::getParam<double>("RootSystem.Grid.InitialT", 14)*24*3600; // days -> s
        orderIdx_ = Dumux::getParam<int>("RootSystem.Grid.orderIdx", 0); // 1
        radiusIdx_ = Dumux::getParam<int>("RootSystem.Grid.radiusIdx", 1);
        ctIdx_ = Dumux::getParam<int>("RootSystem.Grid.ctIdx", 2);
        krIdx_ = Dumux::getParam<int>("RootSystem.Grid.krIdx", 3);
        kxIdx_ = Dumux::getParam<int>("RootSystem.Grid.kxIdx", 4);
        idIdx_ = Dumux::getParam<int>("RootSystem.Grid.idIdx", 0);
        //
        kr_ = InputFileFunction("RootSystem.Conductivity", "Kr", "KrAge", krIdx_, orderIdx_); // [cm/hPa/day] ([day])
        kr_.setVariableScale(1./(24.*3600.)); // [s] -> [day]
        kr_.setFunctionScale(1.e-4/(24.*3600.)); // [cm/hPa/day] -> [m/Pa/s]
        kx_ = InputFileFunction("RootSystem.Conductivity","Kx", "KxAge", kxIdx_, orderIdx_); // [cm^4/hPa/day] ([day])
        kx_.setVariableScale(1./(24.*3600.)); // [s] -> [day]
        kx_.setFunctionScale(1.e-10/(24.*3600.)); // [cm^4/hPa/day] -> [m^4/Pa/s]
        radius_ = InputFileFunction("RootSystem", "Radius", "RadiusAge", radiusIdx_, orderIdx_); // [cm] ([day])
        radius_.setVariableScale(1./(24.*3600.)); // [s] -> [day]
        radius_.setFunctionScale(1.e-2); // [cm] -> [m]
        ct_ = InputFileFunction("RootSystem", "CreationTime", ctIdx_, orderIdx_); // segment creation time [s], optional grid data, no variable
        order_ = InputFileFunction("RootSystem", "Order", orderIdx_, orderIdx_); // [1], optional grid data, no variable
        id_ = InputFileFunction("RootSystem", "Id", idIdx_, idIdx_);
    }

    /*!
     * \brief Return the intrinsic permeability for the current sub-control volume in [m^2].
     *
     * \note Kx has units [m^4/(Pa*s)] so we have to divide by the cross-section area
     *       and multiply with a characteristic viscosity
     */
    template<class ElementSolution>
    PermeabilityType permeability(const Element& element,
        const SubControlVolume& scv, const ElementSolution& elemSol) const {
        return permeability(element);
    }

    //! simpler interface
    PermeabilityType permeability(const Element& element) const {
        Scalar mu = Water::liquidViscosity(285.15, 1e5); // temperature, pressure
        auto eIdx = this->fvGridGeometry().elementMapper().index(element);
        Scalar a = this->radius(eIdx);
        Scalar kx = this->kx(eIdx);
        return kx * mu / (M_PI * a * a);
    }

    //! \brief returns the porosity \f$[-]\f$
    Scalar porosityAtPos(const GlobalPosition& globalPos) const {
        return 1.;
    }

    // id of the polyline the segment belongs to [1]
    Scalar id(std::size_t eIdx) const {
        return id_.f(0.,eIdx);
    }

    //! segment root order, or root type [1], starts at zero
    int order(std::size_t eIdx) const {
        int o = (int)order_.f(eIdx);
        if (o<0) {
            std::cout << "RootSpatialParams::order: warning root order is negative for element index " << eIdx <<": " << o << ", resuming with root order 0\n" << std::flush;
            o = 0;
        }
        return o;
    }

    //! segment radius [m]
    Scalar radius(std::size_t eIdx) const {
        return radius_.f(this->age(eIdx), eIdx);
    }

    //! segment age [s]
    Scalar age(std::size_t eIdx) const {
        return (time0_ - ct_.f(eIdx)) + time_;
    }

    //! radial conductivity [m/Pa/s] == [m^2 s/kg]
    Scalar kr(std::size_t eIdx) const {
        return kr_.f(this->age(eIdx), eIdx);
    }

    //! axial conductivity [m^4/Pa/s]
    Scalar kx(std::size_t eIdx) const {
        return kx_.f(this->age(eIdx), eIdx);
    }

    //! set current simulation time, age is time dependent (so sad), kx and kr can be age dependent
    void setTime(double t, double dt) {
        time_ = t;
        dt_ = dt;
    }

    //! Read initial parameters from grid data object
    template<class GridData>
    void initParameters(const GridData& gridData) {
        const auto& fvGridGeometry = this->fvGridGeometry();
        kr_.setGridData(gridData, fvGridGeometry);
        kx_.setGridData(gridData, fvGridGeometry);
        order_.setGridData(gridData, fvGridGeometry);
        id_.setGridData(gridData, fvGridGeometry);
        radius_.setGridData(gridData, fvGridGeometry);
        ct_.setGridData(gridData, fvGridGeometry);
    }

    //! ignore (because there is no common base class with rootspatialparams_rb.hh)
    template<class RootSystem>
    void updateParameters(const RootSystem& rs) {
        DUNE_THROW(Dune::InvalidStateException, "updateParameters is called for DGF");
    }

private:

    int orderIdx_;
    int radiusIdx_;
    int ctIdx_;
    int krIdx_;
    int kxIdx_;
    int idIdx_;

    InputFileFunction kr_;
    InputFileFunction kx_;
    InputFileFunction order_;
    InputFileFunction radius_;
    InputFileFunction ct_;
    InputFileFunction id_;

    double time_ = 0.;
    double dt_ = 0.;
    double time0_ = 0.; // for calculating age from ct
};

} // end namespace Dumux

#endif
