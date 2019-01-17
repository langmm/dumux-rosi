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
 * \ingroup OnePTests
 * \brief A test problem for the 1p model. A pipe system with circular cross-section
 *        and a branching point embedded in a three-dimensional world
 */
#ifndef ROOTS_PROBLEM_HH
#define ROOTS_PROBLEM_HH

#include <dune/localfunctions/lagrange/pqkfactory.hh>
#include <dune/geometry/quadraturerules.hh>

#if HAVE_DUNE_FOAMGRID
#include <dune/foamgrid/foamgrid.hh>
#endif

#include <dumux/common/reorderingdofmapper.hh>
#include <dumux/discretization/cctpfa.hh>
#include <dumux/discretization/box.hh>
#include <dumux/discretization/method.hh>
#include <dumux/discretization/elementsolution.hh>
#include <dumux/porousmediumflow/1p/model.hh>
#include <dumux/porousmediumflow/problem.hh>
#include <dumux/material/components/constant.hh>
#include <dumux/material/fluidsystems/1pliquid.hh>

#include <dumux/growth/soillookup.hh>

#include <math.h>

#include "rootspatialparams_dgf.hh"
#include "rootspatialparams_rb.hh"

namespace Dumux {

template<class TypeTag>
class RootsProblem;

namespace Properties {

// Create new type tags
namespace TTag {
struct Roots {
    using InheritsFrom = std::tuple<OneP>;
};
struct RootsCCTpfa {
    using InheritsFrom = std::tuple<Roots, CCTpfaModel>;
};
struct RootsBox {
    using InheritsFrom = std::tuple<Roots, BoxModel>;
};
} // end namespace TTag

// Set the grid type
#if HAVE_DUNE_FOAMGRID
template<class TypeTag>
struct Grid<TypeTag, TTag::Roots> {using type = Dune::FoamGrid<1, 3>;};
#endif

// if we have pt scotch use the reordering dof mapper to optimally sort the dofs (cc)
template<class TypeTag>
struct FVGridGeometry<TypeTag, TTag::RootsCCTpfa> {
private:
    static constexpr bool enableCache = getPropValue<TypeTag, Properties::EnableFVGridGeometryCache>();
    using GridView = GetPropType<TypeTag, Properties::GridView>;

    using ElementMapper = ReorderingDofMapper<GridView>;
    using VertexMapper = Dune::MultipleCodimMultipleGeomTypeMapper<GridView>;
    using MapperTraits = DefaultMapperTraits<GridView, ElementMapper, VertexMapper>;
public:
    using type = CCTpfaFVGridGeometry<GridView, enableCache, CCTpfaDefaultGridGeometryTraits<GridView, MapperTraits>>;
};

// if we have pt scotch use the reordering dof mapper to optimally sort the dofs (box)
template<class TypeTag>
struct FVGridGeometry<TypeTag, TTag::RootsBox> {
private:
    static constexpr bool enableCache = getPropValue<TypeTag, Properties::EnableFVGridGeometryCache>();
    using GridView = GetPropType<TypeTag, Properties::GridView>;
    using Scalar = GetPropType<TypeTag, Properties::Scalar>;
    using ElementMapper = Dune::MultipleCodimMultipleGeomTypeMapper<GridView>;
    using VertexMapper = ReorderingDofMapper<GridView>;
    using MapperTraits = DefaultMapperTraits<GridView, ElementMapper, VertexMapper>;
public:
    using type = BoxFVGridGeometry<Scalar, GridView, enableCache, BoxDefaultGridGeometryTraits<GridView, MapperTraits>>;
};

// Set the problem property
template<class TypeTag>
struct Problem<TypeTag, TTag::Roots> {
    using type = RootsProblem<TypeTag>;
};

// the fluid system
template<class TypeTag>
struct FluidSystem<TypeTag, TTag::Roots> {
    using Scalar = GetPropType<TypeTag, Properties::Scalar>;
    using type = FluidSystems::OnePLiquid<Scalar, Components::SimpleH2O<Scalar>>;
};

} // end namespace Properties


/*!
 * \ingroup RootsProblem
 * \brief A test problem for roots
 */
template<class TypeTag>
class RootsProblem: public PorousMediumFlowProblem<TypeTag> {
    using ParentType = PorousMediumFlowProblem<TypeTag>;
    using GridView = GetPropType<TypeTag, Properties::GridView>;
    using Scalar = GetPropType<TypeTag, Properties::Scalar>;
    using ElementVolumeVariables = typename GetPropType<TypeTag, Properties::GridVolumeVariables>::LocalView;

    static const int dim = GridView::dimension;
    static const int dimWorld = GridView::dimensionworld;

    using Indices = typename GetPropType<TypeTag, Properties::ModelTraits>::Indices;
    enum {
        // indices of the primary variables
        conti0EqIdx = Indices::conti0EqIdx,
        pressureIdx = Indices::pressureIdx
    };

    using PrimaryVariables = GetPropType<TypeTag, Properties::PrimaryVariables>;
    using BoundaryTypes = GetPropType<TypeTag, Properties::BoundaryTypes>;
    using NumEqVector = GetPropType<TypeTag, Properties::NumEqVector>;
    using Element = typename GridView::template Codim<0>::Entity;
    using FVGridGeometry = GetPropType<TypeTag, Properties::FVGridGeometry>;
    using SolutionVector = GetPropType<TypeTag, Properties::SolutionVector>;
    using FVElementGeometry = typename GetPropType<TypeTag, Properties::FVGridGeometry>::LocalView;
    using SubControlVolume = typename FVElementGeometry::SubControlVolume;
    using GlobalPosition = typename Element::Geometry::GlobalCoordinate;

    using PointSource = GetPropType<TypeTag, Properties::PointSource>;
    using SubControlVolumeFace = typename FVElementGeometry::SubControlVolumeFace;

    enum {
        isBox = GetPropType<TypeTag, Properties::FVGridGeometry>::discMethod == DiscretizationMethod::box
    };

    enum {
        bcDirichlet = 0, bcNeumann = 1
    };

public:

    RootsProblem(std::shared_ptr<const FVGridGeometry> fvGridGeometry) :
        ParentType(fvGridGeometry) {
        auto sf = InputFileFunction("Soil.IC.P", "Soil.IC.Z");
        soil_ = new SoilLookUpTable<FVGridGeometry>(sf, fvGridGeometry);
        try {
            collar_ = InputFileFunction("RootSystem.Collar.P", "RootSystem.Collar.PT", true);
            bcType_ = bcDirichlet;
        } catch (...) {
            collar_ = InputFileFunction("RootSystem.Collar.Transpiration", "RootSystem.Collar.TranspirationT", true);
            bcType_ = bcNeumann;
        }
        file_at_.open(this->name() + "_actual_transpiration.txt");
    }

    ~RootsProblem() {
        std::cout << "closing file \n";
        file_at_.close();
    }

    //! calculates axial fluxes from a given solution (for vtk output)
    void axialFlux(const SolutionVector& sol) {
        const auto& gridView = this->fvGridGeometry().gridView();
        axialFlux_ = std::vector<Scalar>(gridView.size(0));
        auto eMapper = this->fvGridGeometry().elementMapper();
        auto vMapper = this->fvGridGeometry().vertexMapper();
        for (const auto& element : elements(gridView)) {
            const auto eIdx = eMapper.index(element);
            auto geo = element.geometry();
            auto length = geo.volume();
            auto kx = this->spatialParams().kx(eIdx);
            auto i0 = vMapper.subIndex(element, 0, 1);
            auto i1 = vMapper.subIndex(element, 1, 1);
            axialFlux_[eIdx] = kx * (sol[i1] - sol[i0]) / length; // m^3 / s
            //            std::cout << "element " << eIdx << " has " << geo.corners() << " corners \n";
            //            for (size_t i = 0; i < geo.corners(); i++) {
            //                auto vIdx = vMapper.subIndex(element, i, 1);
            //                std::cout << "   vertex Index " << vIdx << "\n";
            //            }
            // std::cout << " kx " << kx; // 1e-18
        }
        // std::cout << "axial flux vector is " << axialFlux_.size() << "\n";
    }

    //! calculates the radial fluxes from a given solution (for vtk output)
    void radialFlux(const SolutionVector& sol) {
        const auto& gridView = this->fvGridGeometry().gridView();
        radialFlux_ = std::vector<Scalar>(gridView.size(0));
        auto eMapper = this->fvGridGeometry().elementMapper();
        auto vMapper = this->fvGridGeometry().vertexMapper();
        for (const auto& element : elements(gridView)) {
            auto eIdx = eMapper.index(element);
            auto kr = this->spatialParams().kr(eIdx);
            auto i0 = vMapper.subIndex(element, 0, 1);
            auto i1 = vMapper.subIndex(element, 1, 1);
            auto p = element.geometry().center();
            radialFlux_[eIdx] = kr * (soil(p) - (sol[i1] + sol[i0]) / 2); // m^3 / s
            // std::cout << " kr " << kr;
        }
        // std::cout << "radial flux vector is " << radialFlux_.size() << "\n";
    }

    //! evaluates a spatialparams function given for an element eIdx for all elements
    std::vector<Scalar> vtkField(std::function<Scalar(size_t)> f) {
        const auto& gridView = this->fvGridGeometry().gridView();
        size_t n = gridView.size(0);
        std::vector<Scalar> v = std::vector<Scalar>(n);
        for (size_t i = 0; i < n; i++) {
            v[i] = f(i);
        }
        return v;
    }

    std::vector<Scalar>& radialFlux() {
        return radialFlux_;
    }
    std::vector<Scalar>& axialFlux() {
        return axialFlux_;
    }

    // calculates transpiraton, as the netflux of first element (m^3 /s)
    Scalar transpiration(const SolutionVector& sol) {
        const auto& gridView = this->fvGridGeometry().gridView();
        auto eMapper = this->fvGridGeometry().elementMapper();
        auto vMapper = this->fvGridGeometry().vertexMapper();
        // indices
        auto& e = std::begin(elements(gridView)); // first element
        auto geo = e.geometry();
        auto eIdx = eMapper.index(e);
        auto i0 = vMapper.subIndex(e, 0, 1);
        auto i1 = vMapper.subIndex(e, 1, 1);
        // axial flux
        auto length = geo.volume();
        auto kx = this->spatialParams().kx(eIdx);
        Scalar axialFlux = kx * (sol[i1] - sol[i0]) / length; // m^3 / s
        // radial flux
        auto kr = this->spatialParams().kr(eIdx);
        auto p = geo.center();
        Scalar radialFlux = kr * (soil(p) - (sol[i1] + sol[i0]) / 2);
        return axialFlux + radialFlux;
    }

    /*
     * \brief Return the temperature within the domain in [K]. (actually needed? why?)
     */
    Scalar temperature() const {
        return 273.15 + 10; // 10C
    }

    /*!
     * \brief Specifies which kind of boundary condition should be
     *        used for which equation on a given boundary control volume.
     */
    BoundaryTypes boundaryTypesAtPos(const GlobalPosition &pos) const {
        BoundaryTypes bcTypes;
        bcTypes.setAllNeumann(); // default
        if (onUpperBoundary_(pos)) { // root collar
            if (bcType_ == bcDirichlet) {
                bcTypes.setAllDirichlet();
            } else {
                bcTypes.setAllNeumann();
            }
        } else { // for all other (i.e. root tips)
            bcTypes.setAllNeumann();
        }
        return bcTypes;
    }

    /*!
     * \brief Evaluate the boundary conditions for a dirichlet
     *        control volume.
     */
    PrimaryVariables dirichletAtPos(const GlobalPosition &pos) const {
        return PrimaryVariables(collar());
    }

    /*
     * This is the method for the case where the Neumann condition is
     * potentially solution dependent
     *
     * Negative values mean influx.
     * E.g. for the mass balance that would the mass flux in \f$ [ kg / (m^2 \cdot s)] \f$.
     */
    NumEqVector neumann(const Element& element, const FVElementGeometry& fvGeometry, const ElementVolumeVariables& elemVolVars,
        const SubControlVolumeFace& scvf) const {
        const auto globalPos = scvf.center();
        if (onUpperBoundary_(globalPos)) {
            auto& volVars = elemVolVars[scvf.insideScvIdx()];
            auto p = volVars.pressure(0);
            auto eIdx = this->fvGridGeometry().elementMapper().index(element);
            Scalar kx = this->spatialParams().kx(eIdx);
            auto dist = (globalPos - fvGeometry.scv(scvf.insideScvIdx()).center()).two_norm();
            Scalar maxTrans = volVars.density(0) / volVars.viscosity(0) * kx * (p - criticalCollarPressure_) / (dist);
            Scalar trans;
            if (bcType_ == bcDirichlet) {
                trans = volVars.density(0) / volVars.viscosity(0) * kx * (p - collar()) / (dist); //  / volVars.viscosity(0)
                // std::cout << "\nweak dirichlet " << trans << ", max " << maxTrans << " \n";
            } else { // neumann
                trans = collar(); // kg/s
            }
            Scalar v = std::min(trans, maxTrans);
            lastTrans_ = v;
            v /= volVars.extrusionFactor(); // * scvf.area(); // convert from kg/s to kg/(s*m^2)
//            std::cout << "dist: " << dist << "\n";
//            std::cout << "scvf.area(): " << scvf.area() << "\n";
//            std::cout << "viscosity: " << volVars.viscosity(0) << "\n";
//            std::cout << "density(): " << volVars.density(0) << "\n";
//            std::cout << "extrusionFactor(): " << volVars.extrusionFactor() << "\n";
            return NumEqVector(v);;
        } else {
            return NumEqVector(0.);
        }
    }

    /*!
     * For this method, the return parameter stores the conserved quantity rate
     * generated or annihilate per volume unit. Positive values mean
     * that the conserved quantity is created, negative ones mean that it vanishes.
     * E.g. for the mass balance that would be a mass rate in \f$ [ kg / (m^3 \cdot s)] \f$.
     */
    NumEqVector source(const Element &element, const FVElementGeometry& fvGeometry, const ElementVolumeVariables& elemVolVars,
        const SubControlVolume &scv) const {
        NumEqVector values;
        auto params = this->spatialParams();
        const auto eIdx = params.fvGridGeometry().elementMapper().index(element);
        Scalar a = params.radius(eIdx); // root radius (m)
        Scalar kr = params.kr(eIdx); //  radial conductivity (m^2 s / kg)
        Scalar phx = elemVolVars[scv.localDofIndex()].pressure(); // kg/m/s^2
        Scalar phs = soil(scv.center()); // kg/m/s^2
        values[conti0EqIdx] = kr * 2 * a * M_PI * (phs - phx); // m^3/s
        values[conti0EqIdx] /= (a * a * M_PI); // 1/s
        values[conti0EqIdx] *= rho_; // (kg/s/m^3)
        return values;
    }

    /*!
     * \brief Return how much the domain is extruded at a given sub-control volume.
     */
    template<class ElementSolution>
    Scalar extrusionFactor(const Element& element, const SubControlVolume& scv, const ElementSolution& elemSol) const {
        const auto eIdx = this->spatialParams_->fvGridGeometry().elementMapper().index(element);
        Scalar r = this->spatialParams_->radius(eIdx); // root radius (m)
        return M_PI * r * r;
    }

    /*!
     * \brief Evaluate the initial value for a control volume.
     */
    PrimaryVariables initialAtPos(const GlobalPosition& p) const {
        // std::cout << "initial pos " << p[2] << ": " << soil(p) << "\n";
        return PrimaryVariables(soil(p)); // soil(p)
    }

    void setSoil(CRootBox::SoilLookUp* s) {
        std::cout << "manually changed soil to " << s->toString() << "\n";
        soil_ = s;
    }

    //! soil pressure (called by initial, and source term)
    Scalar soil(const GlobalPosition& p) const {
        return toPa_(soil_->getValue(CRootBox::Vector3d(p[0] * 100, p[1] * 100, p[2] * 100)));
    }

    //! sets the current simulation time (within the simulation loop) for collar boundary look up
    void setTime(Scalar t) {
        this->spatialParams().setTime(t);
        time_ = t;
    }

    //! writes the transpiration file
    void writeTranspirationRate() {
        file_at_ << time_ << ", "; //
        file_at_ << lastTrans_ << "\n";
        std::cout << "\nlast transpiration = " << lastTrans_ << " kg/s \n";
    }

    //! pressure or transpiration rate at the root collar (called by dirichletor neumann, respectively)
    Scalar collar() const {
        if (bcType_ == bcDirichlet) {
            return toPa_(collar_.f(time_));
        } else {
            return collar_.f(time_); // TODO: conversions?
        }

    }

private:

    CRootBox::SoilLookUp* soil_; // todo make shared
    InputFileFunction collar_;
    int bcType_;
    Scalar time_ = 0.;
    Scalar criticalCollarPressure_ = toPa_(-1e4);

    Scalar toPa_(Scalar ph) const {     // cm -> Pa
        return pRef_ + ph / 100. * rho_ * g_;
    }

    bool onUpperBoundary_(const GlobalPosition &globalPos) const {  // on root collar
        return globalPos[dimWorld - 1] > this->fvGridGeometry().bBoxMax()[dimWorld - 1] - eps_;
    }

    static constexpr Scalar g_ = 9.81; // cm / s^2
    static constexpr Scalar rho_ = 1.e3; // kg / m^3
    static constexpr Scalar pRef_ = 1.e5; // Pa
    static constexpr Scalar eps_ = 1e-8;

    std::ofstream file_at_; // file for actual transpiration
    mutable Scalar lastTrans_ = 0.;

    // vtk fields
    std::vector<Scalar> axialFlux_;
    std::vector<Scalar> radialFlux_;

};

} //end namespace Dumux

#endif
