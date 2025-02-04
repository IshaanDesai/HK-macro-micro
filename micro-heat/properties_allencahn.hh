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
 * \ingroup PhasefieldTests
 * \brief The properties of the Allen-Cahn phase-field test.
 */
#ifndef DUMUX_PLAINALLENCAHN_PROPERTIES_HH
#define DUMUX_PLAINALLENCAHN_PROPERTIES_HH

#include <dune/grid/yaspgrid.hh>

#include <dumux/common/properties.hh>
#include <dumux/discretization/cctpfa.hh>

#include <dumux/phasefield/model.hh>

#include "problem_allencahn.hh"

namespace Dumux::Properties {

namespace TTag {
struct PlainAllenCahn { using InheritsFrom = std::tuple<Phasefield, CCTpfaModel>; };
}

template<class TypeTag>
struct Grid<TypeTag, TTag::PlainAllenCahn> { using type = Dune::SPGrid<double, 2>; };

template<class TypeTag>
struct Problem<TypeTag, TTag::PlainAllenCahn> { using type = PlainAllenCahnProblem<TypeTag>; };

template<class TypeTag>
struct EnableGridVolumeVariablesCache<TypeTag, TTag::PlainAllenCahn> { static constexpr bool value = true; };

template<class TypeTag>
struct EnableGridGeometryCache<TypeTag, TTag::PlainAllenCahn> { static constexpr bool value = true; };

} // end namespace Dumux::Properties

#endif