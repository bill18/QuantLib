/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004, 2007 StatPro Italia srl

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

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/dividendvanillaoption.hpp>
#include <ql/instruments/impliedvolatility.hpp>
#include <ql/pricingengines/vanilla/analyticdividendeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <memory>

namespace QuantLib {

    DividendVanillaOption::DividendVanillaOption(
                           const std::shared_ptr<StrikedTypePayoff>& payoff,
                           const std::shared_ptr<Exercise>& exercise,
                           const std::vector<Date>& dividendDates,
                           const std::vector<Real>& dividends)
    : OneAssetOption(payoff, exercise),
      cashFlow_(DividendVector(dividendDates, dividends)) {}


    Volatility DividendVanillaOption::impliedVolatility(
             Real targetValue,
             const std::shared_ptr<GeneralizedBlackScholesProcess>& process,
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
            QL_DEPRECATED_DISABLE_WARNING
            engine = std::make_unique<AnalyticDividendEuropeanEngine>(newProcess);
            QL_DEPRECATED_ENABLE_WARNING
            break;
          case Exercise::American:
            engine = std::make_unique<FdBlackScholesVanillaEngine>(newProcess);
            break;
          case Exercise::Bermudan:
            QL_FAIL("engine not available for Bermudan option with dividends");
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


    void DividendVanillaOption::setupArguments(
                                       PricingEngine::arguments* args) const {
        OneAssetOption::setupArguments(args);

        auto* arguments = dynamic_cast<DividendVanillaOption::arguments*>(args);
        QL_REQUIRE(arguments != nullptr, "wrong engine type");

        arguments->cashFlow = cashFlow_;
    }


    void DividendVanillaOption::arguments::validate() const {
        OneAssetOption::arguments::validate();

        Date exerciseDate = exercise->lastDate();

        for (Size i = 0; i < cashFlow.size(); i++) {
            QL_REQUIRE(cashFlow[i]->date() <= exerciseDate,
                       "the " << io::ordinal(i+1) << " dividend date ("
                       << cashFlow[i]->date()
                       << ") is later than the exercise date ("
                       << exerciseDate << ")");
        }
    }

}

