// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   See the file COPYING for full copying permissions.                      *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 3 of the License, or       *
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
//adapted from <dumux/phasefield/localresidual.hh>
//and <dumux/flux/cctpfa/darcyslaw.hh>

#ifndef CELL_PROBLEM_LOCAL_RESIDUAL_HH
#define CELL_PROBLEM_LOCAL_RESIDUAL_HH

#include <dumux/common/properties.hh>
#include <dumux/assembly/cclocalresidual.hh>
#include <dumux/discretization/cellcentered/tpfa/computetransmissibility.hh>

#include <dumux/discretization/method.hh>
#include <dumux/discretization/extrusion.hh>
#include "indices.hh"
#include "volumevariables.hh"

namespace Dumux {

template<class TypeTag>
class CellProblemLocalResidual : public CCLocalResidual<TypeTag>
{
    using ParentType = CCLocalResidual<TypeTag>;
    using Scalar = GetPropType<TypeTag, Properties::Scalar>;
    using Problem = GetPropType<TypeTag, Properties::Problem>;
    using NumEqVector = Dumux::NumEqVector<GetPropType<TypeTag, Properties::PrimaryVariables>>;
    using VolumeVariables = GetPropType<TypeTag, Properties::VolumeVariables>; //or CellProblemVolume>Variables?
    using ElementVolumeVariables = typename GetPropType<TypeTag, Properties::GridVolumeVariables>::LocalView;
    using ElementFluxVariablesCache = typename GetPropType<TypeTag, Properties::GridFluxVariablesCache>::LocalView;
    using FVElementGeometry = typename GetPropType<TypeTag, Properties::GridGeometry>::LocalView;
    using SubControlVolume = typename FVElementGeometry::SubControlVolume;
    using SubControlVolumeFace = typename FVElementGeometry::SubControlVolumeFace;
    using GridView = typename GetPropType<TypeTag, Properties::GridGeometry>::GridView;
    using Element = typename GridView::template Codim<0>::Entity;
    using GridGeometry = GetPropType<TypeTag, Properties::GridGeometry>;
    using Extrusion = Extrusion_t<GridGeometry>;

    using ModelTraits = GetPropType<TypeTag, Properties::ModelTraits>;
    using Indices = typename ModelTraits::Indices;
    static constexpr int numComponents = ModelTraits::numComponents();
    using DimWorldVector = Dune::FieldVector<Scalar, GridView::dimensionworld>;

public:
    using ParentType::ParentType;

    /*!
     * \brief Evaluate the rate of change of all conservation
     *        quantites (e.g. phase mass) within a sub-control volume.
     *        Cell problem is static, ergo storage is zero.
     *
     * \param problem The problem
     * \param scv The sub control volume
     * \param volVars The current or previous volVars
     */
    NumEqVector computeStorage(const Problem& problem,
                               const SubControlVolume& scv,
                               const VolumeVariables& volVars) const
    {   
        //storage[Indices::psi1Idx] = 0.0;
        //storage[Indices::psi2Idx] = 0.0;
        NumEqVector storage(0.0);

        return storage;
    }

    //from cctpfa/darcyslaw, 
    //TODO check for periodic boundary (see dumux-phasefield)
    template<class Problem, class ElementVolumeVariables, class ElementFluxVarsCache>
    NumEqVector computeFlux(const Problem& problem,
                       const Element& element,
                       const FVElementGeometry& fvGeometry,
                       const ElementVolumeVariables& elemVolVars,
                       const SubControlVolumeFace& scvf,
                       const ElementFluxVarsCache& elemFluxVarsCache) const
    {   NumEqVector flux;
        //const auto& fluxVarsCache = elemFluxVarsCache[scvf]; 

        int k = 0; 
        // Get the inside and outside volume variables
        const auto& insideScv = fvGeometry.scv(scvf.insideScvIdx());
        const auto& outsideScv = fvGeometry.scv(scvf.outsideScvIdx());
        const auto& insideVolVars = elemVolVars[scvf.insideScvIdx()]; //insideScv];
        const auto& outsideVolVars = elemVolVars[scvf.outsideScvIdx()];

        // Obtain inside and outside pressures
        const auto valInside = insideVolVars.priVar(k);
        const auto valOutside = outsideVolVars.priVar(k);

        //currently without fluxVarsCache (add functions from Darcyslaw to speed up)
        const auto& tij = calculateTransmissibility(problem, element, fvGeometry, elemVolVars, scvf);
        
        //unit vector in dimension k
        DimWorldVector e_k(0.0); 
        e_k[problem.spatialParams().getPsiIndex()] = -1.0;  //TODO +/-?
        
        const auto alpha_inside = vtmv(scvf.unitOuterNormal(), insideVolVars.phi0delta(problem, element, insideScv), e_k)*insideVolVars.extrusionFactor();

        flux[k] = tij*(valInside - valOutside) + Extrusion::area(fvGeometry, scvf)*alpha_inside;

        if (!scvf.boundary())
        {
            const auto& outsideScv = fvGeometry.scv(scvf.outsideScvIdx());
            const auto outsideK = outsideVolVars.phi0delta(problem, element, outsideScv);
            const auto outsideTi = fvGeometry.gridGeometry().isPeriodic()
                ? computeTpfaTransmissibility(fvGeometry, fvGeometry.flipScvf(scvf.index()), outsideScv, outsideK, outsideVolVars.extrusionFactor())
                : -1.0*computeTpfaTransmissibility(fvGeometry, scvf, outsideScv, outsideK, outsideVolVars.extrusionFactor());
            const auto alpha_outside = vtmv(scvf.unitOuterNormal(), outsideK, e_k)*outsideVolVars.extrusionFactor();

            flux[k] -= tij/outsideTi*(alpha_inside - alpha_outside);
        }
        //}
        return flux;
    }   

    //from dumux/flux/cctpfa/darcyslaw.hh
    // The flux variables cache has to be bound to an element prior to flux calculations
    // During the binding, the transmissibility will be computed and stored using the method below.
    template<class Problem, class ElementVolumeVariables>
    static Scalar calculateTransmissibility(const Problem& problem,
                                            const Element& element,
                                            const FVElementGeometry& fvGeometry,
                                            const ElementVolumeVariables& elemVolVars,
                                            const SubControlVolumeFace& scvf)
    {
        Scalar tij;
 
        const auto insideScvIdx = scvf.insideScvIdx();
        const auto& insideScv = fvGeometry.scv(insideScvIdx);
        const auto& insideVolVars = elemVolVars[insideScvIdx];

        const Scalar ti = computeTpfaTransmissibility(fvGeometry, scvf, insideScv, 
                                                      insideVolVars.phi0delta(problem, element, insideScv), 
                                                      insideVolVars.extrusionFactor());
 
        // on the boundary (dirichlet) we only need ti
        if (scvf.boundary())
            tij = Extrusion::area(fvGeometry, scvf)*ti;
 
        // otherwise we compute a tpfa harmonic mean
        else
        {
            const auto outsideScvIdx = scvf.outsideScvIdx();
            // as we assemble fluxes from the neighbor to our element the outside index
            // refers to the scv of our element, so we use the scv method
            const auto& outsideScv = fvGeometry.scv(outsideScvIdx);
            const auto& outsideVolVars = elemVolVars[outsideScvIdx];
            const Scalar tj = fvGeometry.gridGeometry().isPeriodic()
                ? computeTpfaTransmissibility(fvGeometry, fvGeometry.flipScvf(scvf.index()), outsideScv, outsideVolVars.phi0delta(problem, element, outsideScv), outsideVolVars.extrusionFactor())
                : -1.0*computeTpfaTransmissibility(fvGeometry, scvf, outsideScv, outsideVolVars.phi0delta(problem, element, outsideScv), outsideVolVars.extrusionFactor());
 
            // harmonic mean (check for division by zero!)
            // TODO: This could lead to problems!? Is there a better way to do this?
            if (ti*tj <= 0.0)
                tij = 0.0;
            else
                tij = Extrusion::area(fvGeometry, scvf)*(ti * tj)/(ti + tj);
        }
 
        return tij;
    }
};

} // end namespace Dumux

#endif