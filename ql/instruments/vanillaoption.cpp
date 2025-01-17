/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2007 StatPro Italia srl

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

#include <ql/exercise.hpp>
#include <ql/instruments/impliedvolatility.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/analyticdividendeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <memory>

namespace QuantLib {

    VanillaOption::VanillaOption(
        const std::shared_ptr<StrikedTypePayoff>& payoff,
        const std::shared_ptr<Exercise>& exercise)
    : OneAssetOption(payoff, exercise) {}


    Volatility VanillaOption::impliedVolatility(
             Real targetValue,
             const std::shared_ptr<GeneralizedBlackScholesProcess>& process,
             Real accuracy,
             Size maxEvaluations,
             Volatility minVol,
             Volatility maxVol) const {
        return impliedVolatility(targetValue, process, DividendSchedule(),
                                 accuracy, maxEvaluations, minVol, maxVol);
    }

    Volatility VanillaOption::impliedVolatility(
             Real targetValue,
             const std::shared_ptr<GeneralizedBlackScholesProcess>& process,
             const DividendSchedule& dividends,
             Real accuracy,
             Size maxEvaluations,
             Volatility minVol,
             Volatility maxVol) const {

        QL_REQUIRE(!isExpired(), "option expired");

        std::shared_ptr<SimpleQuote> volQuote(new SimpleQuote);

        std::shared_ptr<GeneralizedBlackScholesProcess> newProcess =
            detail::ImpliedVolatilityHelper::clone(process, volQuote);

        // engines are built-in for the time being
        std::unique_ptr<PricingEngine> engine;
        switch (exercise_->type()) {
          case Exercise::European:
            if (dividends.empty())
                engine = std::make_unique<AnalyticEuropeanEngine>(newProcess);
            else
                engine = std::make_unique<AnalyticDividendEuropeanEngine>(newProcess, dividends);
            break;
          case Exercise::American:
          case Exercise::Bermudan:
            if (dividends.empty())
                engine = std::make_unique<FdBlackScholesVanillaEngine>(newProcess);
            else
                engine = std::make_unique<FdBlackScholesVanillaEngine>(newProcess, dividends);
            break;
          default:
            QL_FAIL("unknown exercise type");
        }

        return detail::ImpliedVolatilityHelper::calculate(*this,
                                                          *engine,
                                                          *volQuote,
                                                          targetValue,
                                                          accuracy,
                                                          maxEvaluations,
                                                          minVol, maxVol);
    }

    void VanillaOption::setupArguments(PricingEngine::arguments* args) const {
        OneAssetOption::setupArguments(args);

        /* this is a workaround in case an engine is used for both vanilla
           and dividend options.  The dividends might have been set by another
           instrument and need to be cleared. */
        QL_DEPRECATED_DISABLE_WARNING
        auto* arguments = dynamic_cast<DividendVanillaOption::arguments*>(args);
        QL_DEPRECATED_ENABLE_WARNING
        if (arguments != nullptr) {
            arguments->cashFlow.clear();
        }
    }
    
}

