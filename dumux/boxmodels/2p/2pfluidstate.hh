// $Id$
/*****************************************************************************
 *   Copyright (C) 2009 by Markus Wolff                                      *
 *   Institute of Hydraulic Engineering                                      *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version, as long as this copyright notice    *
 *   is included in its original form.                                       *
 *                                                                           *
 *   This program is distributed WITHOUT ANY WARRANTY.                       *
 *****************************************************************************/
/*!
 * \file 
 *
 * \brief Gives the phase state.
 */
#ifndef DUMUX_2P_PHASE_STATE_HH
#define DUMUX_2P_PHASE_STATE_HH

#include <dumux/new_material/fluidstate.hh>
#include <dumux/boxmodels/2p/2pproperties.hh>

namespace Dumux
{
/*!
 * \brief Calcultes the phase state from the primary variables in the
 *        2p model.
 */
template <class TypeTag>
class TwoPFluidState : public FluidState<typename GET_PROP_TYPE(TypeTag, PTAG(Scalar)),
                                           TwoPFluidState<TypeTag> >
{
    typedef TwoPFluidState<TypeTag> ThisType;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar))      Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(FluidSystem)) FluidSystem;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(TwoPIndices)) Indices;

    enum {
        wPhaseIdx = Indices::wPhaseIdx,
        nPhaseIdx = Indices::nPhaseIdx,

        wCompIdx = Indices::wPhaseIdx,
        nCompIdx = Indices::nPhaseIdx,
    };

public:
    enum { numPhases = GET_PROP_VALUE(TypeTag, PTAG(NumPhases)) };
    
public:
    void update(Scalar Sn, Scalar pressW, Scalar pressN, Scalar temperature)
    {
        Sn_ = Sn;
        phasePressure_[wPhaseIdx] = pressW;
        phasePressure_[nPhaseIdx] = pressN;
        temperature_=temperature;
        density_[wPhaseIdx] = FluidSystem::phaseDensity(wPhaseIdx,
                                                        temperature,
                                                        pressW,
                                                        *this);
        density_[nPhaseIdx] = FluidSystem::phaseDensity(nPhaseIdx,
                                                        temperature,
                                                        pressN,
                                                        *this);
    }

    /*!
     * \brief Returns the saturation of a phase.
     */
    Scalar saturation(int phaseIdx) const
    { 
        if (phaseIdx == wPhaseIdx)
            return Scalar(1.0) - Sn_;
        else
            return Sn_;
    };

    /*!
     * \brief Returns the mass fraction of a component in a phase.
     */
    Scalar massFrac(int phaseIdx, int compIdx) const
    { 
        if (compIdx == phaseIdx)
            return 1.0;
        return 0;
    }
    
    /*!
     * \brief Returns the molar fraction of a component in a fluid phase.
     */
    Scalar moleFrac(int phaseIdx, int compIdx) const
    {
        return massFrac(phaseIdx, compIdx);
    }
    
    /*!
     * \brief Returns the total concentration of a phase [mol / m^3].
     *
     * This is equivalent to the sum of all component concentrations.
     */
    Scalar totalConcentration(int phaseIdx) const
    { 
        return density_[phaseIdx]/FluidSystem::molarMass(phaseIdx);
    };

    /*!
     * \brief Returns the concentration of a component in a phase [mol / m^3].
     */
    Scalar concentration(int phaseIdx, int compIdx) const
    { 
        if (phaseIdx == compIdx) 
            return totalConcentration(phaseIdx);
        return 0;
    };
        
    /*!
     * \brief Returns the density of a phase [kg / m^3].
     */
    Scalar density(int phaseIdx) const
    { return density_[phaseIdx]; }
    
    /*!
     * \brief Returns mean molar mass of a phase [kg / mol].
     *
     * This is equivalent to the sum of all component molar masses
     * weighted by their respective mole fraction.
     */
    Scalar averageMolarMass(int phaseIdx) const
    { return FluidSystem::molarMass(phaseIdx); };
    
    /*!
     * \brief Returns the partial pressure of a component in the gas phase [Pa].
     */
    Scalar partialPressure(int compIdx) const
    { 
        if (compIdx == wPhaseIdx)
            return 0;
        return phasePressure_[nPhaseIdx];
    }
    
    /*!
     * \brief Returns the pressure of a fluid phase [Pa].
     */
    Scalar phasePressure(int phaseIdx) const
    { return phasePressure_[phaseIdx]; }

    /*!
     * \brief Returns the capillary pressure [Pa]
     */
    Scalar capillaryPressure() const
    { return phasePressure_[nPhaseIdx] - phasePressure_[wPhaseIdx]; }

    /*!
     * \brief Returns the temperature of the fluids [K].
     *
     * Note that we assume thermodynamic equilibrium, so all fluids
     * and the rock matrix exhibit the same temperature.
     */
    Scalar temperature() const
    { return temperature_; };

public:
    Scalar density_[numPhases];
    Scalar phasePressure_[numPhases];
    Scalar temperature_;
    Scalar Sn_;
};

} // end namepace

#endif
