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
#ifndef DUMUX_RICHARDS_PARAMETERS_HH
#define DUMUX_RICHARDS_PARAMETERS_HH

#include <dumux/material/spatialparams/fv.hh>
#include <dumux/material/fluidmatrixinteractions/2p/regularizedvangenuchten.hh>
// #include <dumux/material/fluidmatrixinteractions/2p/vangenuchten.hh>
#include <dumux/material/fluidmatrixinteractions/2p/efftoabslaw.hh>

#include <dumux/material/components/simpleh2o.hh>
#include <dumux/material/fluidsystems/1pliquid.hh>

#include <dumux/io/inputfilefunction.hh>

namespace Dumux {

/*!
 * The SpatialParams class of RichardsProblem
 *
 * supports multiple soil layers (in z-direction),
 * with different VG parameters sets
 *
 */
template<class FVGridGeometry, class Scalar>
class RichardsParams : public FVSpatialParams<FVGridGeometry, Scalar, RichardsParams<FVGridGeometry, Scalar>>
{
public:

    using GridView = typename FVGridGeometry::GridView;
    using FVElementGeometry = typename FVGridGeometry::LocalView;
    using SubControlVolume = typename FVElementGeometry::SubControlVolume;
    using Element = typename GridView::template Codim<0>::Entity;
    using GlobalPosition = typename Element::Geometry::GlobalCoordinate;
    using Water = Components::SimpleH2O<Scalar>;
    using MaterialLaw = EffToAbsLaw<RegularizedVanGenuchten<Scalar>>;
    using MaterialLawParams = typename MaterialLaw::Params;
    using PermeabilityType = Scalar;

    enum { dimWorld = GridView::dimensionworld };

    RichardsParams(std::shared_ptr<const FVGridGeometry> fvGridGeometry)
    : FVSpatialParams<FVGridGeometry, Scalar, RichardsParams<FVGridGeometry, Scalar>>(fvGridGeometry)
    {
        phi_ = 0.45; // Richards equation is independent of phi [1]

        /* SimpleH2O is constant in regard to temperature and reference pressure */
        Scalar mu = Water::liquidViscosity(0.,0.); // Dynamic viscosity: 1e-3 [Pa s]
        Scalar rho = Water::liquidDensity(0.,0.);  // Density: 1000 [kg/m³]

        /* Get Van Genuchten parameters from the input file */
        std::vector<Scalar> Qr = getParam<std::vector<Scalar>>("Soil.VanGenuchten.Qr"); // [1]
        std::vector<Scalar> Qs = getParam<std::vector<Scalar>>("Soil.VanGenuchten.Qs"); // [1]
        std::vector<Scalar> alpha = getParam<std::vector<Scalar>>("Soil.VanGenuchten.Alpha");  // [1/cm]
        std::vector<Scalar> n = getParam<std::vector<Scalar>>("Soil.VanGenuchten.N"); // [1]
        Kc_ = getParam<std::vector<Scalar>>("Soil.VanGenuchten.Ks"); // hydraulic conductivity [cm/day]
        std::transform(Kc_.begin (), Kc_.end (), Kc_.begin (), std::bind1st(std::multiplies<Scalar>(), 1./100./24./3600.)); // convert from [cm/day] to [m/s]
        homogeneous_ = Qr.size()==1; // more than one set of VG parameters?

        // Qr, Qs, alpha, and n goes to the MaterialLaw VanGenuchten
        for (int i=0; i<Qr.size(); i++) {
            materialParams_.push_back(MaterialLawParams());
            materialParams_.at(i).setSwr(Qr.at(i)/phi_); // Qr
            materialParams_.at(i).setSnr(1.-Qs.at(i)/phi_); // Qs
            Scalar a = alpha.at(i) * 100.; // from [1/cm] to [1/m]
            materialParams_.at(i).setVgAlpha(a/(rho*g_)); //  psi*(rho*g) = p  (from [1/m] to [1/Pa])
            materialParams_.at(i).setVgn(n.at(i)); // N
            K_.push_back(Kc_.at(i)*mu/(rho*g_)); // Convert to intrinsic permeability
            // Regularisation parameters
            double eps = 1.e-4;
            materialParams_.at(i).setPcLowSw(eps);
            materialParams_.at(i).setPcHighSw(1. - eps);
            materialParams_.at(i).setKrnLowSw(eps);
            materialParams_.at(i).setKrwHighSw(1 - eps);
        }
        layerIdx_ = Dumux::getParam<int>("Soil.Grid.layerIdx", 1);
        layer_ = InputFileFunction("Soil.Layer", "Number", "Z", layerIdx_, 0); // [1]([m])
    }

    /*!
     * \brief \copydoc FVGridGeometry::porosity
     */
    Scalar porosityAtPos(const GlobalPosition& globalPos) const {
        return phi_;
    }

    /*!
     * \brief \copydoc FVSpatialParamsOneP::permeability
     * [m^2]\
     */
    template<class ElementSolution>
    decltype(auto) permeability(const Element& element, const SubControlVolume& scv, const ElementSolution& elemSol) const {
        return K_.at(index_(element));
    }

    /*
     * \brief Hydraulic conductivites [m/s], called by the problem for conversions
     */
    const Scalar hydraulicConductivity(const Element& element) const {
        return Kc_.at(index_(element));
    }

    /*!
     * \brief \copydoc FVSpatialParamsOneP::materialLawParams
     */
    template<class ElementSolution>
    const MaterialLawParams& materialLawParams(const Element& element,
        const SubControlVolume& scv,
        const ElementSolution& elemSol) const {
        return materialParams_.at(index_(element));
    }

    //! set of VG parameters for the element
    const MaterialLawParams& materialLawParams(const Element& element) const {
        return materialParams_.at(index_(element));
    }

    //! pointer to the soils layer input file function
    InputFileFunction* layerIFF() {
        return &layer_;
    }

private:

    //! returns the index of the soil layer
    size_t index_(const Element& element) const {
        if (homogeneous_) {
            return 0;
        } else {
            auto eIdx = this->fvGridGeometry().elementMapper().index(element);
            Scalar z = element.geometry().center()[dimWorld - 1];
            return size_t(layer_.f(z, eIdx)-1); // layer number starts with 1 in the input file
        }
    }

    Scalar phi_; // porosity


    bool homogeneous_; // soil is homogeneous
    InputFileFunction layer_;
    int layerIdx_; // index of layer data within the grid

    std::vector<Scalar> K_; // permeability [m²]
    std::vector<Scalar> Kc_; // hydraulic conductivity [m/s]
    std::vector<MaterialLawParams> materialParams_;

    static constexpr Scalar g_ = 9.81; // cm / s^2

};

} // end namespace Dumux

#endif
