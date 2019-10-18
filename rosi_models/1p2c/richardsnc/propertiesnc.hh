// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
#ifndef DUMUX_SOIL_PROPERTIES_NC_HH
#define DUMUX_SOIL_PROPERTIES_NC_HH

#include <dune/grid/yaspgrid.hh>
#if HAVE_DUNE_ALUGRID
#include <dune/alugrid/grid.hh>
#endif
#if HAVE_UG
#include <dune/grid/uggrid.hh>
#endif

#include <dumux/discretization/cctpfa.hh>
#include <dumux/discretization/box.hh>

#include <dumux/porousmediumflow/richardsnc/model.hh>

#include <dumux/multidomain/traits.hh>
#include <dumux/multidomain/embedded/couplingmanager1d3d.hh>

#include <RootSystem.h>

namespace Dumux {
namespace Properties {

namespace TTag { // Create new type tags
struct RichardsTT { using InheritsFrom = std::tuple<RichardsNC>; };
struct RichardsBox { using InheritsFrom = std::tuple<RichardsTT, BoxModel>; };
struct RichardsCC { using InheritsFrom = std::tuple<RichardsTT, CCTpfaModel>; };
}

// Set grid type
#ifndef GRIDTYPE
template<class TypeTag>
struct Grid<TypeTag, TTag::RichardsTT> { using type = Dune::YaspGrid<3,Dune::EquidistantOffsetCoordinates<double,3>>; };
#else
template<class TypeTag>
struct Grid<TypeTag, TTag::RichardsTT> { using type = GRIDTYPE; };  // Use GRIDTYPE from CMakeLists.txt
#endif

// Set the physical problem to be solved
template<class TypeTag>
struct Problem<TypeTag, TTag::RichardsTT> { using type = RichardsNCProblem<TypeTag>; };

// Set the spatial parameters
template<class TypeTag>
struct SpatialParams<TypeTag, TTag::RichardsTT> {
    using type = RichardsNCParams<GetPropType<TypeTag, Properties::FVGridGeometry>, GetPropType<TypeTag, Properties::Scalar>>;
};

// Set the physical problem to be solved
//template<class TypeTag>
//struct PointSource<TypeTag, TTag::RichardsTT> { using type = SolDependentPointSource<TypeTag>; };

} // end namespace properties
} // end namespace DUMUX

#endif
