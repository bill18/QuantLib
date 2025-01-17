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


#include <ql/methods/finitedifferences/meshers/fdmblackscholesmesher.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>
#include <ql/methods/finitedifferences/meshers/fdmsimpleprocess1dmesher.hpp>
#include <ql/methods/finitedifferences/operators/fdmlinearoplayout.hpp>
#include <ql/methods/finitedifferences/solvers/fdmcirsolver.hpp>
#include <ql/methods/finitedifferences/stepconditions/fdmstepconditioncomposite.hpp>
#include <ql/methods/finitedifferences/utilities/fdminnervaluecalculator.hpp>
#include <ql/pricingengines/vanilla/fdcirvanillaengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/processes/coxingersollrossprocess.hpp>
#include <utility>

namespace QuantLib {

    FdCIRVanillaEngine::FdCIRVanillaEngine(
        std::shared_ptr<CoxIngersollRossProcess> cirProcess,
        std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess,
        Size tGrid,
        Size xGrid,
        Size rGrid,
        Size dampingSteps,
        const Real rho,
        const FdmSchemeDesc& schemeDesc,
        std::shared_ptr<FdmQuantoHelper> quantoHelper)
    :  bsProcess_(std::move(bsProcess)), cirProcess_(std::move(cirProcess)),
       quantoHelper_(std::move(quantoHelper)), explicitDividends_(false),
       tGrid_(tGrid), xGrid_(xGrid), rGrid_(rGrid), dampingSteps_(dampingSteps),
       rho_(rho), schemeDesc_(schemeDesc) {}

    FdCIRVanillaEngine::FdCIRVanillaEngine(
        std::shared_ptr<CoxIngersollRossProcess> cirProcess,
        std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess,
        DividendSchedule dividends,
        Size tGrid,
        Size xGrid,
        Size rGrid,
        Size dampingSteps,
        const Real rho,
        const FdmSchemeDesc& schemeDesc,
        std::shared_ptr<FdmQuantoHelper> quantoHelper)
    : bsProcess_(std::move(bsProcess)), cirProcess_(std::move(cirProcess)),
      quantoHelper_(std::move(quantoHelper)), dividends_(std::move(dividends)),
      explicitDividends_(true),
      tGrid_(tGrid), xGrid_(xGrid), rGrid_(rGrid), dampingSteps_(dampingSteps), rho_(rho),
      schemeDesc_(schemeDesc) {}

    FdmSolverDesc FdCIRVanillaEngine::getSolverDesc(Real) const {

        // dividends will eventually be moved out of arguments, but for now we need the switch
        QL_DEPRECATED_DISABLE_WARNING
        const DividendSchedule& passedDividends = explicitDividends_ ? dividends_ : arguments_.cashFlow;
        QL_DEPRECATED_ENABLE_WARNING

        const std::shared_ptr<StrikedTypePayoff> payoff =
            std::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        const Time maturity = bsProcess_->time(arguments_.exercise->lastDate());

        // The short rate mesher
        const std::shared_ptr<Fdm1dMesher> shortRateMesher(
            new FdmSimpleProcess1dMesher(rGrid_, cirProcess_, maturity, tGrid_));

        // The equity mesher
        const std::shared_ptr<Fdm1dMesher> equityMesher(
            new FdmBlackScholesMesher(
                xGrid_, bsProcess_, maturity, payoff->strike(),
                Null<Real>(), Null<Real>(), 0.0001, 1.5,
                std::pair<Real, Real>(payoff->strike(), 0.1),
                passedDividends, quantoHelper_,
                0.0));
        
        const std::shared_ptr<FdmMesher> mesher(
            new FdmMesherComposite(equityMesher, shortRateMesher));

        // Calculator
        const std::shared_ptr<FdmInnerValueCalculator> calculator(
                          new FdmLogInnerValue(arguments_.payoff, mesher, 0));

        // Step conditions
        const std::shared_ptr<FdmStepConditionComposite> conditions = 
             FdmStepConditionComposite::vanillaComposite(
                                 passedDividends, arguments_.exercise, 
                                 mesher, calculator,
                                 bsProcess_->riskFreeRate()->referenceDate(),
                                 bsProcess_->riskFreeRate()->dayCounter());

        // Boundary conditions
        const FdmBoundaryConditionSet boundaries;

        // Solver
        FdmSolverDesc solverDesc = { mesher, boundaries, conditions,
                                     calculator, maturity,
                                     tGrid_, dampingSteps_ };

        return solverDesc;
    }

    void FdCIRVanillaEngine::calculate() const {
        const std::shared_ptr<StrikedTypePayoff> payoff =
            std::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);

        std::shared_ptr<FdmCIRSolver> solver(new FdmCIRSolver(
                    Handle<CoxIngersollRossProcess>(cirProcess_),
                    Handle<GeneralizedBlackScholesProcess>(bsProcess_),
                    getSolverDesc(1.5), schemeDesc_,
                    rho_, payoff->strike()));

        const Real r0   = cirProcess_->x0();
        const Real spot = bsProcess_->x0();

        results_.value = solver->valueAt(spot, r0);
        results_.delta = solver->deltaAt(spot, r0);
        results_.gamma = solver->gammaAt(spot, r0);
        results_.theta = solver->thetaAt(spot, r0);
    }

    MakeFdCIRVanillaEngine::MakeFdCIRVanillaEngine(
        std::shared_ptr<CoxIngersollRossProcess> cirProcess,
        std::shared_ptr<GeneralizedBlackScholesProcess> bsProcess,
        const Real rho)
    : cirProcess_(std::move(cirProcess)), bsProcess_(std::move(bsProcess)), rho_(rho),
      schemeDesc_(std::make_shared<FdmSchemeDesc>(FdmSchemeDesc::ModifiedHundsdorfer())) {}

    MakeFdCIRVanillaEngine& MakeFdCIRVanillaEngine::withQuantoHelper(
        const std::shared_ptr<FdmQuantoHelper>& quantoHelper) {
        quantoHelper_ = quantoHelper;
        return *this;
    }

    MakeFdCIRVanillaEngine&
    MakeFdCIRVanillaEngine::withTGrid(Size tGrid) {
        tGrid_ = tGrid;
        return *this;
    }

    MakeFdCIRVanillaEngine&
    MakeFdCIRVanillaEngine::withXGrid(Size xGrid) {
        xGrid_ = xGrid;
        return *this;
    }

    MakeFdCIRVanillaEngine&
    MakeFdCIRVanillaEngine::withRGrid(Size rGrid) {
        rGrid_ = rGrid;
        return *this;
    }

    MakeFdCIRVanillaEngine&
    MakeFdCIRVanillaEngine::withDampingSteps(Size dampingSteps) {
        dampingSteps_ = dampingSteps;
        return *this;
    }

    MakeFdCIRVanillaEngine&
    MakeFdCIRVanillaEngine::withFdmSchemeDesc(
        const FdmSchemeDesc& schemeDesc) {
        schemeDesc_ = std::make_shared<FdmSchemeDesc>(schemeDesc);
        return *this;
    }

    MakeFdCIRVanillaEngine&
    MakeFdCIRVanillaEngine::withCashDividends(
            const std::vector<Date>& dividendDates,
            const std::vector<Real>& dividendAmounts) {
        dividends_ = DividendVector(dividendDates, dividendAmounts);
        explicitDividends_ = true;
        return *this;
    }

    MakeFdCIRVanillaEngine::operator
    std::shared_ptr<PricingEngine>() const {
        if (explicitDividends_) {
            return std::make_shared<FdCIRVanillaEngine>(
                cirProcess_,
                bsProcess_,
                dividends_,
                tGrid_, xGrid_, rGrid_, dampingSteps_,
                rho_,
                *schemeDesc_,
                quantoHelper_);
        } else {
            return std::make_shared<FdCIRVanillaEngine>(
                cirProcess_,
                bsProcess_,
                tGrid_, xGrid_, rGrid_, dampingSteps_,
                rho_,
                *schemeDesc_,
                quantoHelper_);
        }
    }

}
