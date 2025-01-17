/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 Lew Wei Hao

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file fdcirvanillaengine.hpp
    \brief Finite-differences CIR vanilla option engine
*/

#ifndef quantlib_fd_cir_vanilla_engine_hpp
#define quantlib_fd_cir_vanilla_engine_hpp

#include <ql/instruments/dividendvanillaoption.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/methods/finitedifferences/solvers/fdmsolverdesc.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>
#include <ql/processes/coxingersollrossprocess.hpp>
#include <ql/termstructures/volatility/equityfx/localvoltermstructure.hpp>

namespace QuantLib {

    class FdmQuantoHelper;

    QL_DEPRECATED_DISABLE_WARNING

    //! Finite-differences CIR vanilla option engine
    /*! \ingroup vanillaengines

        \test the engine has been tested to converge among different schemes.
    */
    class FdCIRVanillaEngine : public DividendVanillaOption::engine {
        QL_DEPRECATED_ENABLE_WARNING
      public:
        FdCIRVanillaEngine(std::shared_ptr<CoxIngersollRossProcess> cirProcess,
                           std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess,
                           Size tGrid,
                           Size xGrid,
                           Size vGrid,
                           Size dampingSteps,
                           Real rho,
                           const FdmSchemeDesc& schemeDesc = FdmSchemeDesc::ModifiedHundsdorfer(),
                           std::shared_ptr<FdmQuantoHelper> quantoHelper = {});

        FdCIRVanillaEngine(std::shared_ptr<CoxIngersollRossProcess> cirProcess,
                           std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess,
                           DividendSchedule dividends,
                           Size tGrid,
                           Size xGrid,
                           Size vGrid,
                           Size dampingSteps,
                           Real rho,
                           const FdmSchemeDesc& schemeDesc = FdmSchemeDesc::ModifiedHundsdorfer(),
                           std::shared_ptr<FdmQuantoHelper> quantoHelper = {});

        void calculate() const override;

        FdmSolverDesc getSolverDesc(Real equityScaleFactor) const;

      private:
        std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess_;
        std::shared_ptr<CoxIngersollRossProcess> cirProcess_;
        std::shared_ptr<FdmQuantoHelper> quantoHelper_;
        DividendSchedule dividends_;
        bool explicitDividends_;
        const Size tGrid_, xGrid_, rGrid_, dampingSteps_;
        const Real rho_;
        const FdmSchemeDesc schemeDesc_;
    };

    class MakeFdCIRVanillaEngine {
      public:
        explicit MakeFdCIRVanillaEngine(std::shared_ptr<CoxIngersollRossProcess> cirProcess,
                                        std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess,
                                        Real rho);

        MakeFdCIRVanillaEngine& withQuantoHelper(
            const std::shared_ptr<FdmQuantoHelper>& quantoHelper);

        MakeFdCIRVanillaEngine& withTGrid(Size tGrid);
        MakeFdCIRVanillaEngine& withXGrid(Size xGrid);
        MakeFdCIRVanillaEngine& withRGrid(Size rGrid);
        MakeFdCIRVanillaEngine& withDampingSteps(
            Size dampingSteps);

        MakeFdCIRVanillaEngine& withFdmSchemeDesc(
            const FdmSchemeDesc& schemeDesc);

        MakeFdCIRVanillaEngine& withCashDividends(
            const std::vector<Date>& dividendDates,
            const std::vector<Real>& dividendAmounts);

        operator std::shared_ptr<PricingEngine>() const;

      private:
        std::shared_ptr<CoxIngersollRossProcess> cirProcess_;
        std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess_;
        DividendSchedule dividends_;
        bool explicitDividends_ = false;
        const Real rho_;
        Size tGrid_ = 10, xGrid_ = 100, rGrid_ = 100, dampingSteps_ = 0;
        std::shared_ptr<FdmSchemeDesc> schemeDesc_;
        std::shared_ptr<FdmQuantoHelper> quantoHelper_;
    };
}

#endif
