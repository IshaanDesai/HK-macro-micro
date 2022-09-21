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
/**
 * \file
 * \ingroup OnePTests
 * \brief Test for the OnePModel in combination with the NI model for a conduction problem.
 *
 * The simulation domain is a tube with an elevated temperature on the left hand side.
 */

#ifndef DUMUX_1PNI_CONDUCTION_PROBLEM_HH
#define DUMUX_1PNI_CONDUCTION_PROBLEM_HH

#include <cmath>

#include <dumux/common/properties.hh>
#include <dumux/common/parameters.hh>

#include <dumux/common/boundarytypes.hh>
#include <dumux/porousmediumflow/problem.hh>

#include <dumux/material/components/h2o.hh>

#include <dumux-precice/couplingadapter.hh>

namespace Dumux {

/*!
 * \ingroup OnePTests
 * \brief Test for the OnePModel in combination with the NI model for a conduction problem.
 *
 * The simulation domain is a tube with an elevated temperature on the left hand side.
 *
 * Initially the domain is fully saturated with water at a constant temperature.
 * On the left hand side there is a Dirichlet boundary condition with an increased
 * temperature and on the right hand side a Dirichlet boundary with constant pressure,
 * saturation and temperature is applied.
 *
 * The results are compared to an analytical solution for a diffusion process.
 * This problem uses the \ref OnePModel and \ref NIModel model.
 */
template <class TypeTag>
class OnePNIConductionProblem : public PorousMediumFlowProblem<TypeTag>
{
    using ParentType = PorousMediumFlowProblem<TypeTag>;
    using GridView = typename GetPropType<TypeTag, Properties::GridGeometry>::GridView;
    using Scalar = GetPropType<TypeTag, Properties::Scalar>;
    using PrimaryVariables = GetPropType<TypeTag, Properties::PrimaryVariables>;
    using FluidSystem = GetPropType<TypeTag, Properties::FluidSystem>;
    using BoundaryTypes = Dumux::BoundaryTypes<GetPropType<TypeTag, Properties::ModelTraits>::numEq()>;
    using ThermalConductivityModel = GetPropType<TypeTag, Properties::ThermalConductivityModel>;
    using VolumeVariables = GetPropType<TypeTag, Properties::VolumeVariables>; //TODO or replace entirely with MyVolumeVariables
    using SolutionVector = GetPropType<TypeTag, Properties::SolutionVector>;
    using IapwsH2O = Components::H2O<Scalar>;

    enum { dimWorld = GridView::dimensionworld };

    // copy some indices for convenience
    using Indices = typename GetPropType<TypeTag, Properties::ModelTraits>::Indices;
    enum {
        // indices of the primary variables
        pressureIdx = Indices::pressureIdx,
        temperatureIdx = Indices::temperatureIdx
    };

    using Element = typename GridView::template Codim<0>::Entity;
    using GlobalPosition = typename Element::Geometry::GlobalCoordinate;
    using GridGeometry = GetPropType<TypeTag, Properties::GridGeometry>;

public:
    OnePNIConductionProblem(std::shared_ptr<const GridGeometry> gridGeometry, const std::string& paramGroup)
    : ParentType(gridGeometry, paramGroup),
      couplingInterface_(Dumux::Precice::CouplingAdapter::getInstance())
    {
        //initialize fluid system
        FluidSystem::init();

        name_ = getParam<std::string>("Problem.Name");
    }

    int returnTemperatureIdx()
    {
        return temperatureIdx;
    }


    void updatePreciceDataIds(std::map<std::string,int> readDataIDs, int temperatureID) //or do i overwrite temperatureIdx here?
    {
        temperatureId_ = temperatureID;
        porosityId_ = readDataIDs["porosity"];
        //TODO conductivityID
        dataIdsWereSet_ = true;
    }

    /*!
     * \name Problem parameters
     */
    // \{

    /*!
     * \brief The problem name.
     *
     * This is used as a prefix for files generated by the simulation.
     */
    const std::string& name() const
    {
        return name_;
    }
    // \}

    /*!
     * \name Boundary conditions
     */
    // \{

    /*!
     * \brief Specifies which kind of boundary condition should be
     *        used for which equation on a given boundary segment.
     *
     * \param globalPos The position for which the bc type should be evaluated
     */
    BoundaryTypes boundaryTypesAtPos(const GlobalPosition &globalPos) const
    {   BoundaryTypes bcTypes;

        if(globalPos[1] < eps_ || globalPos[1] > this->gridGeometry().bBoxMax()[1] - eps_)
            bcTypes.setAllDirichlet();
        else
            bcTypes.setAllNeumann(); //default is adiabatic

        return bcTypes;
    }

    /*!
     * \brief Evaluates the boundary conditions for a Dirichlet boundary segment.
     *
     * \param globalPos The position for which the bc type should be evaluated
     *
     * For this method, the \a values parameter stores primary variables.
     */
    PrimaryVariables dirichletAtPos(const GlobalPosition &globalPos) const
    {   //TODO currently hardcoded
        PrimaryVariables priVars(initial_());
        if (globalPos[1] < eps_)
            priVars[temperatureIdx] = getParam<Scalar>("BoundaryConditions.BcBottom");
        if (globalPos[1] > this->gridGeometry().bBoxMax()[1] - eps_)
            priVars[temperatureIdx] = getParam<Scalar>("BoundaryConditions.BcTop");
        return priVars;
    }

    // \}

    /*!
     * \name Volume terms
     */
    // \{

    /*!
     * \brief Evaluates the initial value for a control volume.
     *
     * \param globalPos The position for which the initial condition should be evaluated
     *
     * For this method, the \a values parameter stores primary
     * variables.
     */
    PrimaryVariables initialAtPos(const GlobalPosition &globalPos) const
    {
        return initial_(); //called 25times
    }


private:
    Dumux::Precice::CouplingAdapter &couplingInterface_;

    // the internal method for the initial condition
    PrimaryVariables initial_() const
    {
        PrimaryVariables priVars(0.0);
        priVars[pressureIdx] = 1.0e5; //TODO
        priVars[temperatureIdx] = getParam<Scalar>("InitialConditions.Temperature"); //TODO
        return priVars;
    }
    static constexpr Scalar eps_ = 1e-6;
    std::string name_;
    size_t temperatureId_; 
    size_t porosityId_;
    //size_t conductivity TODO
    bool dataIdsWereSet_;
};

} // end namespace Dumux

#endif
