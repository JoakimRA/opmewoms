/*
  Copyright (C) 2008-2013 by Andreas Lauser

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
/*!
 * \file
 *
 * \brief This file contains the necessary classes to calculate the
 *        volumetric fluxes out of a pressure potential gradient using the
 *        Darcy relation.
 */
#ifndef EWOMS_DARCY_FLUX_MODULE_HH
#define EWOMS_DARCY_FLUX_MODULE_HH

#include "multiphasebaseproperties.hh"
#include <ewoms/models/common/quantitycallbacks.hh>

#include <dune/common/fvector.hh>
#include <dune/common/fmatrix.hh>

#include <cmath>

namespace Opm {
namespace Properties {
NEW_PROP_TAG(MaterialLaw);
}}

namespace Ewoms {
template <class TypeTag>
class DarcyIntensiveQuantities;

template <class TypeTag>
class DarcyExtensiveQuantities;

template <class TypeTag>
class DarcyBaseProblem;

/*!
 * \ingroup DarcyFlux
 * \brief Specifies a flux module which uses the Darcy relation.
 */
template <class TypeTag>
struct DarcyFluxModule
{
    typedef DarcyIntensiveQuantities<TypeTag> FluxIntensiveQuantities;
    typedef DarcyExtensiveQuantities<TypeTag> FluxExtensiveQuantities;
    typedef DarcyBaseProblem<TypeTag> FluxBaseProblem;

    /*!
     * \brief Register all run-time parameters for the flux module.
     */
    static void registerParameters()
    { }
};

/*!
 * \ingroup DarcyFlux
 * \brief Provides the defaults for the parameters required by the
 *        Darcy velocity approach.
 */
template <class TypeTag>
class DarcyBaseProblem
{ };

/*!
 * \ingroup DarcyFlux
 * \brief Provides the intensive quantities for the Darcy flux module
 */
template <class TypeTag>
class DarcyIntensiveQuantities
{
    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;
protected:
    void update_(const ElementContext &elemCtx, int dofIdx, int timeIdx)
    { }
};

/*!
 * \ingroup DarcyFlux
 * \brief Provides the Darcy flux module
 *
 * The commonly used Darcy relation looses its validity for Reynolds
 * numbers \f$ Re > 1\f$.  If one encounters flow velocities in porous
 * media above this threshold, the Darcy relation can be
 * used. Like the Darcy relation, it relates the gradient in potential
 * to velocity.  However, this relation is not linear (as in the Darcy
 * case) any more.
 *
 * Therefore, the Newton scheme is used to solve the non-linear
 * relation. This velocity is then used like the Darcy velocity
 * e.g. by the local residual.
 *
 * For Reynolds numbers above \f$\approx 500\f$ the standard Darcy
 * relation also looses it's validity.
 *
 * The Darcy equation is given by the following relation:
 *
 * \f[
  \nabla p_\alpha - \rho_\alpha \vec{g} =
  - \frac{\mu_\alpha}{k_{r,\alpha} K}\vec{v}_\alpha
  - \frac{\rho_\alpha C_E}{\eta_{r,\alpha} \sqrt{K}}
  \left| \vec{v}_\alpha \right| \vec{v}_\alpha
 \f]
 *
 * Where \f$C_E\f$ is the modified Ergun parameter and
 * \f$\eta_{r,\alpha}\f$ is the passability which is given by a
 * closure relation.
 */
template <class TypeTag>
class DarcyExtensiveQuantities
{
    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, ExtensiveQuantities) Implementation;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
    typedef typename GET_PROP_TYPE(TypeTag, MaterialLaw) MaterialLaw;

    enum { dimWorld = GridView::dimensionworld };
    enum { numPhases = GET_PROP_VALUE(TypeTag, NumPhases) };

    typedef Dune::FieldVector<Scalar, dimWorld> DimVector;
    typedef Dune::FieldMatrix<Scalar, dimWorld, dimWorld> DimMatrix;

public:
    /*!
     * \brief Returns the intrinsic permeability tensor for a given
     *        sub-control volume face.
     */
    const DimMatrix &intrinsicPermability() const
    { return K_; }

    /*!
     * \brief Return the pressure potential gradient of a fluid phase
     *        at the face's integration point [Pa/m]
     *
     * \param phaseIdx The index of the fluid phase
     */
    const DimVector &potentialGrad(int phaseIdx) const
    { return potentialGrad_[phaseIdx]; }

    /*!
     * \brief Return the filter velocity of a fluid phase at the
     *        face's integration point [m/s]
     *
     * \param phaseIdx The index of the fluid phase
     */
    const DimVector &filterVelocity(int phaseIdx) const
    { return filterVelocity_[phaseIdx]; }

    /*!
     * \brief Return the volume flux of a fluid phase at the face's integration point
     *        \f$[m^3/s / m^2]\f$
     *
     * This is the fluid volume of a phase per second and per square meter of face
     * area.
     *
     * \param phaseIdx The index of the fluid phase
     */
    Scalar volumeFlux(int phaseIdx) const
    { return volumeFlux_[phaseIdx]; }

protected:
    short upstreamIndex_(int phaseIdx) const
    { return upstreamDofIdx_[phaseIdx]; }

    short downstreamIndex_(int phaseIdx) const
    { return downstreamDofIdx_[phaseIdx]; }

    /*!
     * \brief Calculate the gradients which are required to determine the volumetric fluxes
     *
     * The the upwind directions is also determined by method.
     */
    void calculateGradients_(const ElementContext &elemCtx,
                             int faceIdx,
                             int timeIdx)
    {
        const auto& gradCalc = elemCtx.gradientCalculator();
        Ewoms::PressureCallback<TypeTag> pressureCallback(elemCtx);

        const auto& scvf = elemCtx.stencil(timeIdx).interiorFace(faceIdx);
        const auto &faceNormal = scvf.normal();

        interiorDofIdx_ = scvf.interiorIndex();
        exteriorDofIdx_ = scvf.exteriorIndex();

        // calculate the "raw" pressure gradient
        for (int phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            if (!elemCtx.model().phaseIsConsidered(phaseIdx)) {
                Valgrind::SetUndefined(potentialGrad_[phaseIdx]);
                continue;
            }

            pressureCallback.setPhaseIndex(phaseIdx);
            gradCalc.calculateGradient(potentialGrad_[phaseIdx],
                                       elemCtx,
                                       faceIdx,
                                       pressureCallback);
        }

        // correct the pressure gradients by the gravitational acceleration
        if (EWOMS_GET_PARAM(TypeTag, bool, EnableGravity)) {
            // estimate the gravitational acceleration at a given SCV face
            // using the arithmetic mean
            const auto& gIn = elemCtx.problem().gravity(elemCtx, interiorDofIdx_, timeIdx);
            const auto& gEx = elemCtx.problem().gravity(elemCtx, exteriorDofIdx_, timeIdx);

            const auto &intQuantsIn = elemCtx.intensiveQuantities(interiorDofIdx_, timeIdx);
            const auto &intQuantsEx = elemCtx.intensiveQuantities(exteriorDofIdx_, timeIdx);

            const auto &posIn = elemCtx.pos(interiorDofIdx_, timeIdx);
            const auto &posEx = elemCtx.pos(exteriorDofIdx_, timeIdx);
            const auto &posFace = scvf.integrationPos();

            // the distance between the centers of the control volumes
            DimVector distVecIn(posIn);
            DimVector distVecEx(posEx);
            DimVector distVecTotal(posEx);

            distVecIn -= posFace;
            distVecEx -= posFace;
            distVecTotal -= posIn;
            Scalar absDistTotalSquared = distVecTotal.two_norm2();
            for (int phaseIdx=0; phaseIdx < numPhases; phaseIdx++) {
                if (!elemCtx.model().phaseIsConsidered(phaseIdx))
                    continue;

                // calculate the hydrostatic pressure at the integration point of the face
                Scalar rhoIn = intQuantsIn.fluidState().density(phaseIdx);
                Scalar rhoEx = intQuantsEx.fluidState().density(phaseIdx);

                Scalar pStatIn = - rhoIn*(gIn*distVecIn);
                Scalar pStatEx = - rhoEx*(gEx*distVecEx);

                // compute the hydrostatic gradient between the two control volumes (this
                // gradient exhibitis the same direction as the vector between the two
                // control volume centers and the length (pStaticExterior -
                // pStaticInterior)/distanceInteriorToExterior
                auto f(distVecTotal);
                f *= (pStatEx - pStatIn)/absDistTotalSquared;

                // calculate the final potential gradient
                potentialGrad_[phaseIdx] += f;

                if (!std::isfinite(potentialGrad_[phaseIdx].two_norm())) {
                    OPM_THROW(Opm::NumericalProblem,
                              "Non finite potential gradient for phase '"
                              << FluidSystem::phaseName(phaseIdx) << "'");
                }
            }
        } // if (enableGravity)

        Valgrind::SetUndefined(K_);
        elemCtx.problem().intersectionIntrinsicPermeability(K_, elemCtx, faceIdx, timeIdx);
        Valgrind::CheckDefined(K_);

        for (int phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            if (!elemCtx.model().phaseIsConsidered(phaseIdx)) {
                Valgrind::SetUndefined(potentialGrad_[phaseIdx]);
                continue;
            }

            // determine the upstream and downstream DOFs
            Scalar tmp = 0;
            for (unsigned i = 0; i < faceNormal.size(); ++i)
                tmp += potentialGrad_[phaseIdx][i]*faceNormal[i];

            if (tmp > 0) {
                upstreamDofIdx_[phaseIdx] = exteriorDofIdx_;
                downstreamDofIdx_[phaseIdx] = interiorDofIdx_;
            }
            else {
                upstreamDofIdx_[phaseIdx] = interiorDofIdx_;
                downstreamDofIdx_[phaseIdx] = exteriorDofIdx_;
            }

            const auto &up = elemCtx.intensiveQuantities(upstreamDofIdx_[phaseIdx], timeIdx);
            mobility_[phaseIdx] = up.mobility(phaseIdx);
        }
    }

    /*!
     * \brief Calculate the gradients at the grid boundary which are required to
     *        determine the volumetric fluxes
     *
     * The the upwind directions is also determined by method.
     */
    template <class FluidState>
    void calculateBoundaryGradients_(const ElementContext &elemCtx,
                                     int boundaryFaceIdx,
                                     int timeIdx,
                                     const FluidState& fluidState,
                                     const typename FluidSystem::ParameterCache &paramCache)
    {
        const auto& gradCalc = elemCtx.gradientCalculator();
        Ewoms::BoundaryPressureCallback<TypeTag, FluidState> pressureCallback(elemCtx, fluidState);

        // calculate the pressure gradient
        for (int phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            if (!elemCtx.model().phaseIsConsidered(phaseIdx)) {
                Valgrind::SetUndefined(potentialGrad_[phaseIdx]);
                continue;
            }

            pressureCallback.setPhaseIndex(phaseIdx);
            gradCalc.calculateBoundaryGradient(potentialGrad_[phaseIdx],
                                               elemCtx,
                                               boundaryFaceIdx,
                                               pressureCallback);
        }

        const auto& scvf = elemCtx.stencil(timeIdx).boundaryFace(boundaryFaceIdx);
        interiorDofIdx_ = scvf.interiorIndex();
        exteriorDofIdx_ = -1;

        // calculate the intrinsic permeability
        const auto &intQuantsIn = elemCtx.intensiveQuantities(interiorDofIdx_, timeIdx);
        K_ = intQuantsIn.intrinsicPermeability();

        // correct the pressure gradients by the gravitational acceleration
        if (EWOMS_GET_PARAM(TypeTag, bool, EnableGravity)) {
            // estimate the gravitational acceleration at a given SCV face
            // using the arithmetic mean
            const auto& gIn = elemCtx.problem().gravity(elemCtx, interiorDofIdx_, timeIdx);
            const auto& posIn = elemCtx.pos(interiorDofIdx_, timeIdx);
            const auto& posFace = scvf.integrationPos();

            // the distance between the face center and the center of the control volume
            DimVector distVecIn(posIn);
            distVecIn -= posFace;

            Scalar absDist = distVecIn.two_norm();

            for (int phaseIdx=0; phaseIdx < numPhases; phaseIdx++) {
                if (!elemCtx.model().phaseIsConsidered(phaseIdx))
                    continue;

                // calculate the hydrostatic pressure at the integration point of the face
                Scalar rhoIn = intQuantsIn.fluidState().density(phaseIdx);
                Scalar pStatIn = - rhoIn*(gIn*distVecIn);

                // compute the hydrostatic gradient between the two control volumes (this
                // gradient exhibitis the same direction as the vector between the two
                // control volume centers and the length (pStaticExterior -
                // pStaticInterior)/distanceInteriorToExterior
                auto f(distVecIn);
                f *= (0 - pStatIn)/(absDist);

                // calculate the final potential gradient
                potentialGrad_[phaseIdx] += f;

                if (!std::isfinite(potentialGrad_[phaseIdx].two_norm())) {
                    OPM_THROW(Opm::NumericalProblem,
                               "Non finite potential gradient for phase '"
                               << FluidSystem::phaseName(phaseIdx) << "'");
                }
            }
        }

        // determine the upstream and downstream DOFs
        const auto &faceNormal = scvf.normal();

        const auto &matParams = elemCtx.problem().materialLawParams(elemCtx, interiorDofIdx_, timeIdx);

        Scalar kr[numPhases];
        MaterialLaw::relativePermeabilities(kr, matParams, fluidState);

        for (int phaseIdx=0; phaseIdx < numPhases; phaseIdx++) {
            if (!elemCtx.model().phaseIsConsidered(phaseIdx))
                continue;

            Scalar tmp = 0;
            for (unsigned i = 0; i < faceNormal.size(); ++i)
                tmp += potentialGrad_[phaseIdx][i]*faceNormal[i];

            if (tmp > 0) {
                upstreamDofIdx_[phaseIdx] = exteriorDofIdx_;
                downstreamDofIdx_[phaseIdx] = interiorDofIdx_;
            }
            else {
                upstreamDofIdx_[phaseIdx] = interiorDofIdx_;
                downstreamDofIdx_[phaseIdx] = exteriorDofIdx_;
            }

            // take the phase mobility from the DOF in upstream direction
            if (upstreamDofIdx_[phaseIdx] < 0)
                mobility_[phaseIdx] =
                    kr[phaseIdx] / FluidSystem::viscosity(fluidState, paramCache, phaseIdx);
            else
                mobility_[phaseIdx] = intQuantsIn.mobility(phaseIdx);
        }
    }

    /*!
     * \brief Calculate the volumetric fluxes of all phases
     *
     * The pressure potentials and upwind directions must already be
     * determined before calling this method!
     */
    void calculateFluxes_(const ElementContext& elemCtx, int scvfIdx, int timeIdx)
    {
        const auto &scvf = elemCtx.stencil(timeIdx).interiorFace(scvfIdx);
        const DimVector &normal = scvf.normal();
        Valgrind::CheckDefined(normal);

        for (int phaseIdx=0; phaseIdx < numPhases; phaseIdx++) {
            if (!elemCtx.model().phaseIsConsidered(phaseIdx)) {
                filterVelocity_[phaseIdx] = 0;
                volumeFlux_[phaseIdx] = 0;
                continue;
            }

            asImp_().calculateFilterVelocity_(phaseIdx);
            volumeFlux_[phaseIdx] = (filterVelocity_[phaseIdx] * normal);
        }
    }

    /*!
     * \brief Calculate the volumetric fluxes at a boundary face of all fluid phases
     *
     * The pressure potentials and upwind directions must already be determined before
     * calling this method!
     */
    void calculateBoundaryFluxes_(const ElementContext& elemCtx,
                                      int boundaryFaceIdx,
                                      int timeIdx)
    {
        const auto &scvf = elemCtx.stencil(timeIdx).boundaryFace(boundaryFaceIdx);
        const DimVector &normal = scvf.normal();
        Valgrind::CheckDefined(normal);

        for (int phaseIdx=0; phaseIdx < numPhases; phaseIdx++) {
            if (!elemCtx.model().phaseIsConsidered(phaseIdx)) {
                filterVelocity_[phaseIdx] = 0;
                volumeFlux_[phaseIdx] = 0;
                continue;
            }

            asImp_().calculateFilterVelocity_(phaseIdx);
            volumeFlux_[phaseIdx] = (filterVelocity_[phaseIdx] * normal);
        }
    }

    void calculateFilterVelocity_(int phaseIdx)
    {
        K_.mv(potentialGrad_[phaseIdx], filterVelocity_[phaseIdx]);
        filterVelocity_[phaseIdx] *= - mobility_[phaseIdx];
    }

private:
    Implementation &asImp_()
    { return *static_cast<Implementation*>(this); }

    const Implementation &asImp_() const
    { return *static_cast<const Implementation*>(this); }

protected:
    // intrinsic permeability tensor and its square root
    DimMatrix K_;

    // interior, exterior, upstream and downstream DOFs
    short interiorDofIdx_;
    short exteriorDofIdx_;
    short upstreamDofIdx_[numPhases];
    short downstreamDofIdx_[numPhases];

    // mobilities of all fluid phases [1 / (Pa s)]
    Scalar mobility_[numPhases];

    // filter velocities of all phases [m/s]
    DimVector filterVelocity_[numPhases];

    // the volumetric flux of all fluid phases over the control
    // volume's face [m^3/s / m^2]
    Scalar volumeFlux_[numPhases];

    // pressure potential gradients of all phases [Pa / m]
    DimVector potentialGrad_[numPhases];
};

} // namespace Ewoms

#endif