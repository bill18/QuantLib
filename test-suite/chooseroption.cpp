/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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

#include "chooseroption.hpp"
#include "utilities.hpp"
#include <ql/time/daycounters/actual360.hpp>
#include <ql/experimental/exoticoptions/simplechooseroption.hpp>
#include <ql/experimental/exoticoptions/complexchooseroption.hpp>
#include <ql/experimental/exoticoptions/analyticsimplechooserengine.hpp>
#include <ql/experimental/exoticoptions/analyticcomplexchooserengine.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

#undef REPORT_FAILURE
#define REPORT_FAILURE(greekName, choosingDate, \
                       exercise, s, q, r, today, v, \
                       expected, calculated, tolerance) \
    BOOST_ERROR( \
        exerciseTypeToString(exercise) \
        << " Chooser option with " \
        << "    spot value: " << s << "\n" \
        << "    dividend yield:   " << io::rate(q) << "\n" \
        << "    risk-free rate:   " << io::rate(r) << "\n" \
        << "    reference date:   " << today << "\n" \
        << "    maturity:         " << exercise->lastDate() << "\n" \
        << "    volatility:       " << io::volatility(v) << "\n\n" \
        << "    expected   " << greekName << ": " << expected << "\n" \
        << "    calculated " << greekName << ": " << calculated << "\n"\
        << "    error:            " << std::fabs(expected-calculated) \
        << "\n" \
        << "    tolerance:        " << tolerance);


void ChooserOptionTest::testAnalyticSimpleChooserEngine(){

    BOOST_TEST_MESSAGE("Testing analytic simple chooser option...");

    /* The data below are from
       "Complete Guide to Option Pricing Formulas", Espen Gaarder Haug
       pages 39-40
    */
    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();

    std::shared_ptr<SimpleQuote> spot = std::make_shared<SimpleQuote>(50.0);
    std::shared_ptr<SimpleQuote> qRate = std::make_shared<SimpleQuote>(0.0);
    std::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    std::shared_ptr<SimpleQuote> rRate = std::make_shared<SimpleQuote>(0.08);
    std::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    std::shared_ptr<SimpleQuote> vol = std::make_shared<SimpleQuote>(0.25);
    std::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    std::shared_ptr<BlackScholesMertonProcess> stochProcess =
        std::make_shared<BlackScholesMertonProcess>(
                                  Handle<Quote>(spot),
                                  Handle<YieldTermStructure>(qTS),
                                  Handle<YieldTermStructure>(rTS),
                                  Handle<BlackVolTermStructure>(volTS));

    std::shared_ptr<PricingEngine> engine =
        std::make_shared<AnalyticSimpleChooserEngine>(stochProcess);

    Real strike = 50.0;

    Date exerciseDate = today + 180;
    std::shared_ptr<Exercise> exercise =
        std::make_shared<EuropeanExercise>(exerciseDate);

    Date choosingDate = today + 90;
    SimpleChooserOption option(choosingDate,strike,exercise);
    option.setPricingEngine(engine);

    Real calculated = option.NPV();
    Real expected = 6.1071;
    Real tolerance = 3e-5;
    if (std::fabs(calculated-expected) > tolerance) {
        REPORT_FAILURE("value", choosingDate,
                       exercise, spot->value(),
                       qRate->value(), rRate->value(), today,
                       vol->value(), expected, calculated, tolerance);
    }

}


void ChooserOptionTest::testAnalyticComplexChooserEngine(){
    BOOST_TEST_MESSAGE("Testing analytic complex chooser option...");

    /* The example below is from
       "Complete Guide to Option Pricing Formulas", Espen Gaarder Haug
    */
    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    std::shared_ptr<SimpleQuote> spot = std::make_shared<SimpleQuote>(50.0);
    std::shared_ptr<SimpleQuote> qRate = std::make_shared<SimpleQuote>(0.05);
    std::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    std::shared_ptr<SimpleQuote> rRate = std::make_shared<SimpleQuote>(0.10);
    std::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    std::shared_ptr<SimpleQuote> vol = std::make_shared<SimpleQuote>(0.35);
    std::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    std::shared_ptr<BlackScholesMertonProcess> stochProcess =
        std::make_shared<BlackScholesMertonProcess>(
                                  Handle<Quote>(spot),
                                  Handle<YieldTermStructure>(qTS),
                                  Handle<YieldTermStructure>(rTS),
                                  Handle<BlackVolTermStructure>(volTS));

    std::shared_ptr<PricingEngine> engine =
        std::make_shared<AnalyticComplexChooserEngine>(stochProcess);

    Real callStrike = 55.0;
    Real putStrike = 48.0;

    Date choosingDate = today + 90;
    Date callExerciseDate = choosingDate + 180;
    Date putExerciseDate = choosingDate + 210;
    std::shared_ptr<Exercise> callExercise =
        std::make_shared<EuropeanExercise>(callExerciseDate);
    std::shared_ptr<Exercise> putExercise =
        std::make_shared<EuropeanExercise>(putExerciseDate);

    ComplexChooserOption option(choosingDate,callStrike,putStrike,
                                callExercise,putExercise);
    option.setPricingEngine(engine);

    Real calculated = option.NPV();
    Real expected = 6.0508;
    Real error = std::fabs(calculated-expected);
    Real tolerance = 1e-4;
    if (error > tolerance) {
        BOOST_ERROR("Failed to reproduce complex chooser option value"
                    << "\n    expected:   " << expected
                    << "\n    calculated: " << calculated
                    << "\n    error:      " << error);
    }
}

test_suite* ChooserOptionTest::suite() {
    auto* suite = BOOST_TEST_SUITE("Chooser option tests");

    suite->add(QUANTLIB_TEST_CASE(
        &ChooserOptionTest::testAnalyticSimpleChooserEngine));
    suite->add(QUANTLIB_TEST_CASE(
        &ChooserOptionTest::testAnalyticComplexChooserEngine));

    return suite;
}
