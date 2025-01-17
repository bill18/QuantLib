/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2005 Joseph Wang
 Copyright (C) 2007, 2009 StatPro Italia srl

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

/*! \file fddividendengine.hpp
    \brief base engine for option with dividends
*/

#ifndef quantlib_fd_dividend_engine_hpp
#define quantlib_fd_dividend_engine_hpp

#include <ql/pricingengines/vanilla/fdmultiperiodengine.hpp>
#include <ql/instruments/dividendvanillaoption.hpp>

namespace QuantLib {

    /*! \deprecated Use the new finite-differences framework instead.
                    Deprecated in version 1.27.
    */
    template <template <class> class Scheme = CrankNicolson>
    class QL_DEPRECATED FDDividendEngineBase : public FDMultiPeriodEngine<Scheme> {
      public:
        FDDividendEngineBase(
             const std::shared_ptr<GeneralizedBlackScholesProcess>& process,
             Size timeSteps = 100,
             Size gridPoints = 100,
             bool timeDependent = false)
        : FDMultiPeriodEngine<Scheme>(process, timeSteps,
                                      gridPoints, timeDependent) {}
      protected:
        virtual void setupArguments(const PricingEngine::arguments*) const;
        virtual void setGridLimits() const = 0;
        virtual void executeIntermediateStep(Size step) const = 0;
        Real getDividendAmount(Size i) const {
            const auto* dividend = dynamic_cast<const Dividend*>(this->events_[i].get());
            if (dividend != nullptr) {
                return dividend->amount();
            } else {
                return 0.0;
            }
        }
        Real getDiscountedDividend(Size i) const {
            Real dividend = getDividendAmount(i);
            Real discount =
                this->process_->riskFreeRate()->discount(
                                                   this->events_[i]->date()) /
                this->process_->dividendYield()->discount(
                                                    this->events_[i]->date());
            return dividend * discount;
        }
    };

    QL_DEPRECATED_DISABLE_WARNING

    /*! \deprecated Use the new finite-differences framework instead.
                    Deprecated in version 1.27.
    */
    template <template <class> class Scheme = CrankNicolson>
    class QL_DEPRECATED FDDividendEngineMerton73 : public FDDividendEngineBase<Scheme> {
      public:
        FDDividendEngineMerton73(
             const std::shared_ptr<GeneralizedBlackScholesProcess>& process,
             Size timeSteps = 100,
             Size gridPoints = 100,
             bool timeDependent = false)
        : FDDividendEngineBase<Scheme>(process, timeSteps,
                                       gridPoints, timeDependent) {}
      private:
        void setGridLimits() const;
        void executeIntermediateStep(Size step) const;
    };

    /*! \deprecated Use the new finite-differences framework instead.
                    Deprecated in version 1.27.
    */
    template <template <class> class Scheme = CrankNicolson>
    class QL_DEPRECATED FDDividendEngineShiftScale : public FDDividendEngineBase<Scheme> {
      public:
        FDDividendEngineShiftScale(
             const std::shared_ptr<GeneralizedBlackScholesProcess>& process,
             Size timeSteps = 100,
             Size gridPoints = 100,
             bool timeDependent = false)
        : FDDividendEngineBase<Scheme>(process, timeSteps,
                                       gridPoints, timeDependent) {}
      private:
        void setGridLimits() const;
        void executeIntermediateStep(Size step) const;
    };


    /*! \deprecated Use the new finite-differences framework instead.
                    Deprecated in version 1.27.
    */
    template <template <class> class Scheme = CrankNicolson>
    class QL_DEPRECATED FDDividendEngine : public FDDividendEngineMerton73<Scheme> {
      public:
        FDDividendEngine(
             const std::shared_ptr<GeneralizedBlackScholesProcess>& process,
             Size timeSteps = 100,
             Size gridPoints = 100,
             bool timeDependent = false)
        : FDDividendEngineMerton73<Scheme>(process, timeSteps,
                                           gridPoints, timeDependent) {}
    };


    // template definitions

    template <template <class> class Scheme>
    void FDDividendEngineBase<Scheme>::setupArguments(
                                    const PricingEngine::arguments *a) const {
        const auto* args = dynamic_cast<const DividendVanillaOption::arguments*>(a);
        QL_REQUIRE(args, "incorrect argument type");
        std::vector<std::shared_ptr<Event> > events(args->cashFlow.size());
        std::copy(args->cashFlow.begin(), args->cashFlow.end(),
                  events.begin());
        FDMultiPeriodEngine<Scheme>::setupArguments(a, events);
    }


    // The value of the x axis is the NPV of the underlying minus the
    // value of the paid dividends.

    // Note that to get the PDE to work, I have to scale the values
    // and not shift them.  This means that the price curve assumes
    // that the dividends are scaled with the value of the underlying.
    //

    template <template <class> class Scheme>
    void FDDividendEngineMerton73<Scheme>::setGridLimits() const {
        Real paidDividends = 0.0;
        for (Size i=0; i<this->events_.size(); i++) {
            if (this->getDividendTime(i) >= 0.0)
                paidDividends += this->getDiscountedDividend(i);
        }

        FDVanillaEngine::setGridLimits(
                       this->process_->stateVariable()->value()-paidDividends,
                       this->getResidualTime());
        this->ensureStrikeInGrid();
    }

    // TODO:  Make this work for both fixed and scaled dividends
    template <template <class> class Scheme>
    void FDDividendEngineMerton73<Scheme>::executeIntermediateStep(
                                                             Size step) const{
        Real scaleFactor =
            this->getDiscountedDividend(step) / this->center_ + 1.0;
        this->sMin_ *= scaleFactor;
        this->sMax_ *= scaleFactor;
        this->center_ *= scaleFactor;

        this->intrinsicValues_.scaleGrid(scaleFactor);
        this->intrinsicValues_.sample(*(this->payoff_));
        this->prices_.scaleGrid(scaleFactor);
        this->initializeOperator();
        this->initializeModel();

        this->initializeStepCondition();
        this->stepCondition_ -> applyTo(this->prices_.values(),
                                        this->getDividendTime(step));
    }

    namespace detail {

        class QL_DEPRECATED DividendAdder {
          private:
            const Dividend *dividend;
          public:
            explicit DividendAdder (const Dividend *d) : dividend(d) {}
            Real operator() (Real x) const {
                return x + dividend->amount(x);
            }
        };

    }

    template <template <class> class Scheme>
    void FDDividendEngineShiftScale<Scheme>::setGridLimits() const {
        Real underlying = this->process_->stateVariable()->value();
        for (Size i=0; i<this->events_.size(); i++) {
            const auto* dividend = dynamic_cast<const Dividend*>(this->events_[i].get());
            if (dividend == nullptr)
                continue;
            if (this->getDividendTime(i) < 0.0) continue;
            underlying -= dividend->amount(underlying);
        }

        FDVanillaEngine::setGridLimits(underlying,
                                       this->getResidualTime());
        this->ensureStrikeInGrid();
    }

    template <template <class> class Scheme>
    void FDDividendEngineShiftScale<Scheme>::executeIntermediateStep(
                                                             Size step) const{
        const auto* dividend = dynamic_cast<const Dividend*>(this->events_[step].get());
        if (dividend == nullptr)
            return;
        detail::DividendAdder adder(dividend);
        this->sMin_ = adder(this->sMin_);
        this->sMax_ = adder(this->sMax_);
        this->center_ = adder(this->center_);
        this->intrinsicValues_.transformGrid(adder);

        this->intrinsicValues_.sample(*(this->payoff_));
        this->prices_.transformGrid(adder);

        this->initializeOperator();
        this->initializeModel();

        this->initializeStepCondition();
        this->stepCondition_ -> applyTo(this->prices_.values(),
                                        this->getDividendTime(step));
    }

    QL_DEPRECATED_ENABLE_WARNING

}


#endif
