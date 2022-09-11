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
/*!
 * \file
 * \ingroup OnePTests
 * \brief Definition of the spatial parameters for the 1pni problems.
 */

#ifndef DUMUX_TEST_1PNI_SPATIAL_PARAMS_HH
#define DUMUX_TEST_1PNI_SPATIAL_PARAMS_HH

#include <dumux/porousmediumflow/properties.hh>
#include <dumux/porousmediumflow/fvspatialparams1p.hh>

#include <dumux-precice/couplingadapter.hh>

namespace Dumux {

/*!
 * \ingroup OnePTests
 * \brief Definition of the spatial parameters for the 1pni problems.
 */
template<class GridGeometry, class Scalar>
class OnePNISpatialParams
: public FVPorousMediumFlowSpatialParamsOneP<GridGeometry, Scalar,
                                         OnePNISpatialParams<GridGeometry, Scalar>>
{
    using GridView = typename GridGeometry::GridView;
    using ThisType = OnePNISpatialParams<GridGeometry, Scalar>;
    using ParentType = FVPorousMediumFlowSpatialParamsOneP<GridGeometry, Scalar, ThisType>;

    static const int dimWorld = GridView::dimensionworld;
    using Element = typename GridView::template Codim<0>::Entity;
    using GlobalPosition = typename Element::Geometry::GlobalCoordinate;

    using FVElementGeometry = typename GridGeometry::LocalView;
    using SubControlVolume = typename FVElementGeometry::SubControlVolume;

public:
    // export permeability type
    using PermeabilityType = Scalar;

    OnePNISpatialParams(std::shared_ptr<const GridGeometry> gridGeometry)
    : ParentType(gridGeometry),
      couplingInterface_(Dumux::Precice::CouplingAdapter::getInstance())
     {}

    /*!
     * \brief Defines the intrinsic permeability \f$\mathrm{[m^2]}\f$.
     *
     * \param globalPos The global position
     */
    PermeabilityType permeabilityAtPos(const GlobalPosition& globalPos) const
    {return 1e-10; } //TODO Does this also vary? 

    /*!
     * \brief Defines the porosity \f$\mathrm{[-]}\f$.
     *
     * \param globalPos The global position
     */
    
    template<class ElementSolution>
    Scalar porosity(const Element& element, 
                    const SubControlVolume& scv, 
                    const ElementSolution& elemSol) const
    {   //TODO make sure element indices correspond to quantityvector
        //TODO check WHEN this function is called (should be AFTER communication)
        //Alternative: via writeToFace in main,.
        double porElement = 0.0;
        std::vector<double> porosityData = couplingInterface_.getQuantityVector(porosityId_);
        for (int i = 0; i != 4; i++){
            porElement += porosityData[scv.elementIndex()*4+i];
        }
        return porElement/4;
    } 

    Scalar solidThermalConductivity(const Element &element,
                                    const FVElementGeometry &fvGeometry,
                                    const SubControlVolume& scv) const
    //TODO:Conductivity implement conductivity tensor here (ignore the "solid" misnomer; scalar to tensor?)
    { return lambdaSolid_; }

private:
    Dumux::Precice::CouplingAdapter &couplingInterface_;
    Scalar lambdaSolid_;
    int porosityId_ = 0; //readDataIDs["porosity"] TODO check, also this is hardcoded in an ugly way
};

} // end namespace Dumux

#endif
