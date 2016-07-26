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
*
* \brief Adaption of the fully implicit scheme to the RichardsTwoC model.
*/
#ifndef DUMUX_RICHARDS_2C_MODEL_HH
#define DUMUX_RICHARDS_2C_MODEL_HH

#include <dune/common/version.hh>
#include <dumux/implicit/model.hh>
#include <dumux/porousmediumflow/implicit/velocityoutput.hh>

#include "richards2cproblem.hh"
#include "richards2cproperties.hh"

namespace Dumux
{
/*!
 * \ingroup RichardsTwoCModel
 *
 * \brief This model which implements a variant of the Richards
 *        equation for quasi-twophase flow with 2 component transport
 *
 */

template<class TypeTag >
class RichardsTwoCModel : public GET_PROP_TYPE(TypeTag, BaseModel)
{
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, FVElementGeometry) FVElementGeometry;
    typedef typename GET_PROP_TYPE(TypeTag, ElementVolumeVariables) ElementVolumeVariables;
    typedef typename GET_PROP_TYPE(TypeTag, SolutionVector) SolutionVector;
    typedef typename GET_PROP_TYPE(TypeTag, PrimaryVariables) PrimaryVariables;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;

    typedef typename GET_PROP_TYPE(TypeTag, Indices) Indices;
    enum {
        nPhaseIdx = Indices::nPhaseIdx,
        wPhaseIdx = Indices::wPhaseIdx
    };

    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GridView::template Codim<0>::Iterator ElementIterator;
    enum { dim = GridView::dimension };
    enum { dimWorld = GridView::dimensionworld };

    enum { isBox = GET_PROP_VALUE(TypeTag, ImplicitIsBox) };
    enum { dofCodim = isBox ? dim : 0 };

public:
    /*!
     * \brief All relevant primary and secondary of a given
     *        solution to an ouput writer.
     *
     * \param sol The current solution which ought to be written to disk
     * \param writer The Dumux::VtkMultiWriter which is be used to write the data
     */
    template <class MultiWriter>
    void addOutputVtkFields(const SolutionVector &sol, MultiWriter &writer)
    {
        typedef Dune::BlockVector<Dune::FieldVector<double, 1> > ScalarField;
        typedef Dune::BlockVector<Dune::FieldVector<double, dimWorld> > VectorField;

        // get the number of degrees of freedom
        unsigned numDofs = this->numDofs();

        // create the required scalar fields
        ScalarField *pw = writer.allocateManagedBuffer(numDofs);
        ScalarField *pn = writer.allocateManagedBuffer(numDofs);
        ScalarField *pc = writer.allocateManagedBuffer(numDofs);
        ScalarField *sw = writer.allocateManagedBuffer(numDofs);
        ScalarField *sn = writer.allocateManagedBuffer(numDofs);
        ScalarField *rhoW = writer.allocateManagedBuffer(numDofs);
        ScalarField *rhoN = writer.allocateManagedBuffer(numDofs);
        ScalarField *mobW = writer.allocateManagedBuffer(numDofs);
        ScalarField *mobN = writer.allocateManagedBuffer(numDofs);
        ScalarField *poro = writer.allocateManagedBuffer(numDofs);
        ScalarField *Te = writer.allocateManagedBuffer(numDofs);
        ScalarField *ph = writer.allocateManagedBuffer(numDofs);
        ScalarField *wc = writer.allocateManagedBuffer(numDofs);

        ScalarField *moleFraction0 = writer.allocateManagedBuffer(numDofs);
        ScalarField *moleFraction1 = writer.allocateManagedBuffer(numDofs);
        ScalarField *massFraction0 = writer.allocateManagedBuffer(numDofs);
        ScalarField *massFraction1 = writer.allocateManagedBuffer(numDofs);

        VectorField *velocity = writer.template allocateManagedBuffer<double, dimWorld>(numDofs);
        ImplicitVelocityOutput<TypeTag> velocityOutput(this->problem_());

        if (velocityOutput.enableOutput())
        {
            // initialize velocity field
            for (unsigned int i = 0; i < numDofs; ++i)
            {
                (*velocity)[i] = Scalar(0);
            }
        }

        unsigned numElements = this->gridView_().size(0);
        ScalarField *rank = writer.allocateManagedBuffer (numElements);

        ElementIterator eIt = this->gridView_().template begin<0>();
        ElementIterator eEndIt = this->gridView_().template end<0>();
        for (; eIt != eEndIt; ++eIt)
        {
            if(eIt->partitionType() == Dune::InteriorEntity)
            {
#if DUNE_VERSION_NEWER(DUNE_COMMON, 2, 4)
                int eIdx = this->problem_().model().elementMapper().index(*eIt);
#else
                int eIdx = this->problem_().model().elementMapper().map(*eIt);
#endif
                (*rank)[eIdx] = this->gridView_().comm().rank();

                FVElementGeometry fvGeometry;
                fvGeometry.update(this->gridView_(), *eIt);

                ElementVolumeVariables elemVolVars;
                elemVolVars.update(this->problem_(),
                                   *eIt,
                                   fvGeometry,
                                   false /* oldSol? */);

                for (int scvIdx = 0; scvIdx < fvGeometry.numScv; ++scvIdx)
                {
#if DUNE_VERSION_NEWER(DUNE_COMMON, 2, 4)
                    int dofIdxGlobal = this->dofMapper().subIndex(*eIt, scvIdx, dofCodim);
#else
                    int dofIdxGlobal = this->dofMapper().map(*eIt, scvIdx, dofCodim);
#endif
                    (*pw)[dofIdxGlobal] = elemVolVars[scvIdx].pressure(wPhaseIdx);
                    (*pn)[dofIdxGlobal] = elemVolVars[scvIdx].pressure(nPhaseIdx);
                    (*pc)[dofIdxGlobal] = elemVolVars[scvIdx].capillaryPressure();
                    (*sw)[dofIdxGlobal] = elemVolVars[scvIdx].saturation(wPhaseIdx);
                    (*sn)[dofIdxGlobal] = elemVolVars[scvIdx].saturation(nPhaseIdx);
                    (*rhoW)[dofIdxGlobal] = elemVolVars[scvIdx].density(wPhaseIdx);
                    (*rhoN)[dofIdxGlobal] = elemVolVars[scvIdx].density(nPhaseIdx);
                    (*mobW)[dofIdxGlobal] = elemVolVars[scvIdx].mobility(wPhaseIdx);
                    (*mobN)[dofIdxGlobal] = elemVolVars[scvIdx].mobility(nPhaseIdx);
                    (*poro)[dofIdxGlobal] = elemVolVars[scvIdx].porosity();
                    (*Te)[dofIdxGlobal] = elemVolVars[scvIdx].temperature();
                    (*ph)[dofIdxGlobal] = elemVolVars[scvIdx].pressureHead(wPhaseIdx);
                    (*wc)[dofIdxGlobal] = elemVolVars[scvIdx].waterContent(wPhaseIdx);
                    (*moleFraction0)[dofIdxGlobal] = elemVolVars[scvIdx].moleFraction(0);
                    (*moleFraction1)[dofIdxGlobal] = elemVolVars[scvIdx].moleFraction(1);
                    (*massFraction0)[dofIdxGlobal] = elemVolVars[scvIdx].massFraction(0);
                    (*massFraction1)[dofIdxGlobal] = elemVolVars[scvIdx].massFraction(1);
                }

                // velocity output
                velocityOutput.calculateVelocity(*velocity, elemVolVars, fvGeometry, *eIt, /*phaseIdx=*/0);
            }
        }

        writer.attachDofData(*sn, "Sn", isBox);
        writer.attachDofData(*sw, "Sw", isBox);
        writer.attachDofData(*pn, "pn", isBox);
        writer.attachDofData(*pw, "pw", isBox);
        writer.attachDofData(*pc, "pc", isBox);
        writer.attachDofData(*rhoW, "rhoW", isBox);
        writer.attachDofData(*rhoN, "rhoN", isBox);
        writer.attachDofData(*mobW, "mobW", isBox);
        writer.attachDofData(*mobN, "mobN", isBox);
        writer.attachDofData(*poro, "porosity", isBox);
        writer.attachDofData(*Te, "temperature", isBox);
        writer.attachDofData(*ph, "pressure head", isBox);
        writer.attachDofData(*wc, "water content", isBox);

        char nameMoleFraction0[42], nameMoleFraction1[42];
        snprintf(nameMoleFraction0, 42, "x_%s", FluidSystem::componentName(0));
        snprintf(nameMoleFraction1, 42, "x_%s", FluidSystem::componentName(1));
        writer.attachDofData(*moleFraction0, nameMoleFraction0, isBox);
        writer.attachDofData(*moleFraction1, nameMoleFraction1, isBox);

        char nameMassFraction0[42], nameMassFraction1[42];
        snprintf(nameMassFraction0, 42, "X_%s", FluidSystem::componentName(0));
        snprintf(nameMassFraction1, 42, "X_%s", FluidSystem::componentName(1));
        writer.attachDofData(*massFraction0, nameMassFraction0, isBox);
        writer.attachDofData(*massFraction1, nameMassFraction1, isBox);

        if (velocityOutput.enableOutput())
        {
            writer.attachDofData(*velocity,  "velocity", isBox, dim);
        }
        writer.attachCellData(*rank, "process rank");
    }

    /*!
     * \brief returns source/sink values of the model - used by coupled approaches
     *
     * \param sourceValues The current source values which are returned
     */
    void sourceValues(SolutionVector& sourcevalues)
    {
        int gridsize = this->gridView_().size(0);
        sourcevalues.resize(gridsize);

        ElementIterator eIt = this->gridView_().template begin<0>();
        ElementIterator eEndIt = this->gridView_().template end<0>();
        for (; eIt != eEndIt; ++eIt)
            {
            int eIdx = this->problem_().model().elementMapper().index(*eIt);

            FVElementGeometry fvGeometry;
            fvGeometry.update(this->gridView_(), *eIt);

            ElementVolumeVariables elemVolVars;
            elemVolVars.update(this->problem_(),
                               *eIt,
                               fvGeometry,
                               false /* oldSol? */);

            PrimaryVariables values;

            for (int scvIdx = 0; scvIdx < fvGeometry.numScv; ++scvIdx)
                {
                    this->problem_().solDependentSource(values,
                                                        *eIt,
                                                        fvGeometry,
                                                        scvIdx,
                                                        elemVolVars);
                }
            sourcevalues[eIdx][0] = values;
            }
    }
};
}

#include "richards2cpropertydefaults.hh"

#endif
