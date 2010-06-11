// $Id$
/*****************************************************************************
 *   Copyright (C) 2008 by Andreas Lauser, Bernd Flemisch                    *
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
 * \file vangenuchtenparams.hh Specification of the material params
 *       for the van Genuchten capillary pressure model.
 */
#ifndef VAN_GENUCHTEN_PARAMS_HH
#define VAN_GENUCHTEN_PARAMS_HH

namespace Dumux
{
/*!
 * \brief Reference implementation of a van Genuchten params
 */
template<class ScalarT>
class VanGenuchtenParams
{
public:
    typedef ScalarT Scalar;

    VanGenuchtenParams()
    {}

    VanGenuchtenParams(Scalar vgAlpha, Scalar vgN)
    {
        setVgAlpha(vgAlpha);
        setVgN(vgN);
    };

    /*!
     * \brief Return the \f$\alpha\f$ shape parameter of van Genuchten's
     *        curve. 
     */
    Scalar vgAlpha() const
    { return vgAlpha_; }

    /*!
     * \brief Set the \f$\alpha\f$ shape parameter of van Genuchten's
     *        curve. 
     */
    void setVgAlpha(Scalar v)
    { vgAlpha_ = v; }

    /*!
     * \brief Return the \f$m\f$ shape parameter of van Genuchten's
     *        curve. 
     */
    Scalar vgM() const
    { return vgM_; }

    /*!
     * \brief Set the \f$m\f$ shape parameter of van Genuchten's
     *        curve. 
     *
     * The \f$n\f$ shape parameter is set to \f$n = \frac{1}{1 - m}\f$ 
     */
    void setVgM(Scalar m) 
    { vgM_ = m; vgN_ = 1/(1 - vgM_); }

    /*!
     * \brief Return the \f$n\f$ shape parameter of van Genuchten's
     *        curve. 
     */
    Scalar vgN() const 
    { return vgN_; }

    /*!
     * \brief Set the \f$n\f$ shape parameter of van Genuchten's
     *        curve. 
     *
     * The \f$n\f$ shape parameter is set to \f$m = 1 - \frac{1}{n}\f$ 
     */
    void setVgN(Scalar n)
    { vgN_ = n; vgM_ = 1 - 1/vgN_; }

private:
    Scalar vgAlpha_;
    Scalar vgM_;
    Scalar vgN_;
};
} // namespace Dumux

#endif
