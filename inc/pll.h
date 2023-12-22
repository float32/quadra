// MIT License
//
// Copyright 2013 Émilie Gillet
// Copyright 2021 Tyler Coy
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include <optional>
#include "util.h"

namespace quadra
{

class PhaseLockedLoop
{
protected:
    float nominal_frequency_;
    float step_;
    float phase_;
    float error_;
    float accumulator_;
    float prev_phase_;

public:
    void Init(float normalized_frequency)
    {
        nominal_frequency_ = normalized_frequency;
        Reset();
    }

    void Reset(void)
    {
        step_ = nominal_frequency_;
        phase_ = 0;
        error_ = 0;
        accumulator_ = 0;
        prev_phase_ = 0;
    }

    float phase(void)
    {
        return phase_;
    }

    float step(void)
    {
        return step_;
    }

    float error(void)
    {
        return error_;
    }

    // If our phase crossed the given threshold phi, return the calculated
    // fractional sample delay between the crossing and the current sample.
    std::optional<float> phase_trigger(float phi)
    {
        if (Wrap(phase_ - phi) < Wrap(prev_phase_ - phi) &&
            phase_ != prev_phase_)
        {
            return Wrap(phase_ - phi) / Wrap(phase_ - prev_phase_);
        }
        else
        {
            return std::nullopt;
        }
    }

    static constexpr float kKp = 0.02;
    static constexpr float kKi = 200e-6;
    static constexpr float kWindupLimit = 0.1;

    void ProcessError(float error)
    {
        error_ = error;

        accumulator_ += kKi * error_;
        accumulator_ = Clamp(accumulator_, -kWindupLimit, kWindupLimit);

        float p_error = kKp * error_;
        float i_error = accumulator_;

        step_ = nominal_frequency_ * (1 - p_error - i_error);
        step_ = Clamp<float>(step_, 0, 1);
    }

    void Step(void)
    {
        prev_phase_ = phase_;
        phase_ = FractionalPart(phase_ + step_);
    }
};

}
