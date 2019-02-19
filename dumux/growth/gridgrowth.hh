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
 * \brief This file contains a growth algorithm using crootbox as a backend
 */
#ifndef DUMUX_GRIDGROWTH_HH
#define DUMUX_GRIDGROWTH_HH

#include <memory>

#include <dune/common/version.hh>
#include <dune/common/exceptions.hh>
#include <dune/grid/common/exceptions.hh>
#include <dune/geometry/type.hh>
#include <dune/grid/utility/persistentcontainer.hh>
#include <dumux/common/properties.hh>
#include <dumux/common/entitymap.hh>

#include "growthinterface.hh" // holds base class, and crootbox interface

namespace Dumux {

namespace GrowthModule {

/**
 * A class managing the growth of the grid (Dune::FoamGrid<1,3>) according to a some grow model GrowthInterface
 */
template<class TypeTag>
class GridGrowth {

    using Grid = GetPropType<TypeTag, Properties::Grid>;
    using FVGridGeometry = GetPropType<TypeTag, Properties::FVGridGeometry>;
    using SolutionVector = GetPropType<TypeTag, Properties::SolutionVector>;
    using PrimaryVariables = GetPropType<TypeTag, Properties::PrimaryVariables>;
    using GridView = GetPropType<TypeTag, Properties::GridView>;
    using Element = typename GridView::template Codim<0>::Entity;
    using GlobalPosition = typename Element::Geometry::GlobalCoordinate;
    using PersistentContainer = Dune::PersistentContainer<Grid, PrimaryVariables>; // todo remove macros
    using Growth = GrowthInterface<GlobalPosition>*;

public:

    //! constructs the grow
    GridGrowth(std::shared_ptr<Grid> grid, std::shared_ptr<FVGridGeometry> fvGridGeometry, Growth growth, SolutionVector& sol) :
            grid_(grid),
            fvGridGeometry_(fvGridGeometry),
            growth_(growth),
            indexToVertex_(*grid, fvGridGeometry->vertexMapper()),
            data_(*grid, 0),
            sol_(sol) {
        const auto& gv = grid->leafGridView();
        indexMap_.resize(gv.size(Grid::dimension));  // TODO: we assume that in the beginning the node indices are the same
        std::iota(indexMap_.begin(), indexMap_.end(), 0);
    }

    //! \param dt the time step size in seconds
    void grow(double dt) {

        // remember the old segment and vertex amount
        const auto& gv = grid_->leafGridView();
        const auto oldNumSegments = gv.size(0);
        const auto oldNumVertices = gv.size(Grid::dimension);

        //! store the old grid data
        storeData_();

        //! grow the root system
        growth_->simulate(dt);

        //! get nodes that changed their position
        auto updatedNodeIndices = growth_->updatedNodeIndices();
        auto updatedNodes = growth_->updatedNodes();
        assert((updatedNodeIndices.size() == updatedNodes.size()) && "GridGrowth: number of updated nodes <> updated indices"); // a little trust...
        for (unsigned int i = 0; i < updatedNodeIndices.size(); i++) {
            grid_->setPosition(indexToVertex_[getDuneIndex_(updatedNodeIndices[i])], updatedNodes[i]);
        }

        //! nodes and segments that have be added
        auto newNodes = growth_->newNodes();
        auto newNodeIndices = growth_->newNodeIndices();
        auto newSegments = growth_->newSegments();

        //! add them to the dune-foamgrid
        if (!newSegments.empty()) {

            // register new vertices
            std::vector<size_t> newNodeIndicesDune(newNodes.size());
            for (size_t i = 0; i < newNodes.size(); i++) {
                newNodeIndicesDune[i] = grid_->insertVertex(newNodes[i]);
                //! Do a check that newly inserted vertices are numbered consecutively in both dune and the growth model
                if (newNodeIndicesDune[i] != newNodeIndices[i]) {
                    DUNE_THROW(Dune::GridError, "Nodes are not consecutively numbered!");
                }
            }

            // insert the line elements
            for (size_t i = 0; i < newSegments.size(); i++) {
                const auto& s = newSegments[i];
                grid_->insertElement(Dune::GeometryTypes::line, { getDuneIndex_(s[0]), getDuneIndex_(s[1]) });
            }

            // this actually inserts the new vertices and segments
            grid_->preGrow();
            grid_->grow();

            // update grid geometry (updates the mappers)
            fvGridGeometry_->update();

            // update the index map
            indexMap_.resize(gv.size(Grid::dimension));
            // update the actual index the new vertices got in the grid in the growth model index to dune index map
            for (const auto& vertex : vertices(gv)) {
                if (fvGridGeometry_->vertexMapper().index(vertex) >= oldNumVertices)
                    indexMap_[grid_->growthInsertionIndex(vertex)] = fvGridGeometry_->vertexMapper().index(vertex);
            }
            for (const auto& e : elements(gv)) {
                auto eIdx = grid_->growthInsertionIndex(e);
            }

            // also update the index to vertex map
            indexToVertex_.update(fvGridGeometry_->vertexMapper());

            // restore the old grid data and initialize new data
            reconstructData_();

            // cleanup grid after growth
            grid_->postGrow();

            std::cout << "addded " <<  newSegments.size() << " segments \n";
        }

        // check if all segments could be inserted
        if (oldNumSegments + newSegments.size() != gv.size(0))
            DUNE_THROW(Dune::GridError, "Not all segments could be inserted!");

        checkIndex();

    }

    //! TODO dune element index from a root model node index
    size_t duneIndex(size_t rIdx) const {
        return indexMap_.at(rIdx);
    }


private:

    /*!
     * for debugging
     */
    void checkIndex() {
        int uneq = 0;
        for (size_t i=0; i<indexMap_.size(); i++) {
            if (indexMap_[i]!=i) {
                uneq++;
            }
        }
        std::cout << "indexMap: " << uneq << " unequal entries \n";
    }


    /*!
     * dune vertex index from a root model node index
     */
    unsigned int getDuneIndex_(int cRootBoxIdx) {
        if (cRootBoxIdx >= indexMap_.size()) {
            return static_cast<unsigned int>(cRootBoxIdx); //! new vertices have to same numbering during insertion
        } else {
            return indexMap_[cRootBoxIdx];
        }
    }

    /*!
     * Store the primary variables of the old grid into data_
     */
    void storeData_() {
        data_.resize();
        const auto& gv = grid_->leafGridView();
        for (const auto& element : elements(gv)) {
            const auto eIdx = fvGridGeometry_->elementMapper().index(element);
            data_[element] = sol_[eIdx];
        }
    }

    /*!
     * Reconstruct missing primary variables (where elements are created/deleted)
     * \param problem The current problem
     */
    void reconstructData_() {
        data_.resize();
        const auto& gv = grid_->leafGridView();
        sol_.resize(gv.size(0));

        for (const auto& element : elements(gv)) { // old elements get their old variables assigned
            if (!element.isNew()) { // get your primary variables from the map
                const auto eIdx = fvGridGeometry_->elementMapper().index(element);
                sol_[eIdx] = data_[element];
            } else { // initialize new elements with the pressure of the surrounding soil (todo)
                const auto newEIdx = fvGridGeometry_->elementMapper().index(element);
                const auto insertionIdx = grid_->growthInsertionIndex(element);
                sol_[newEIdx] = -10000; // todo
            }
        }

        // reset entries in restrictionmap
        data_.resize(typename PersistentContainer::Value());
        data_.shrinkToFit();
        data_.fill(typename PersistentContainer::Value());
    }

    std::shared_ptr<Grid> grid_; //! the dune-foamgrid
    std::shared_ptr<FVGridGeometry> fvGridGeometry_; //! fv grid geometry
    Growth growth_;

    // index mapping stuff
    EntityMap<Grid, Grid::dimension> indexToVertex_; //! a map from dune vertex indices to vertex entitites (?)
    std::vector<size_t> indexMap_; //! a map from crootbox node indices to foamgrid indices

    PersistentContainer data_; //! data container with persistent ids as key to transfer element data from old to new grid

    SolutionVector& sol_; // the data (non-const) to transfer from the old to the new grid
};

} // end namespace GridGrowth

} // end namespace Dumux

#endif
