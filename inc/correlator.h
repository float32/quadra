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
#include "window.h"
#include "util.h"

namespace quadra
{

class Correlator
{
protected:
    static constexpr uint32_t kPatternLength = 8;
    static constexpr float kAlignmentPattern[2][kPatternLength] =
    {
        { -1, -1, -1,  0,  1,  1,  1,  0},
        { -1,  0,  1,  1,  1,  0, -1, -1},
    };

    static constexpr float kPeakThreshold = kPatternLength / 2.0;
    static constexpr uint32_t kNumCorrelationPeaks = 4;

    Window<Vector, kPatternLength> v_history_;
    Window<float, 3> phase_history_;
    Window<float, 3> correlation_history_;
    float maximum_;
    uint32_t correlation_peaks_;
    Window<float, kNumCorrelationPeaks> decision_vector_;

public:
    void Init(void)
    {
        Reset();
    }

    void Reset(void)
    {
        v_history_.Init();
        phase_history_.Init();
        correlation_history_.Init();
        maximum_ = 0;
        correlation_peaks_ = 0;
        decision_vector_.Init();
    }

    void Push(float phase, Vector v)
    {
        phase_history_.Write(phase);
        v_history_.Write(v);
    }

    std::optional<float> Process(float phase, Vector v)
    {
        Push(phase, v);

        float correlation = 0;

        for (uint32_t i = 0; i < kPatternLength; i++)
        {
            float expected_i = kAlignmentPattern[0][i];
            float expected_q = kAlignmentPattern[1][i];
            correlation += expected_i * v_history_[i].real();
            correlation += expected_q * v_history_[i].imag();
        }

        if (correlation > maximum_)
        {
            maximum_ = correlation;
        }

        // Detect a local maximum in the output of the correlator.
        correlation_history_.Write(correlation);

        bool peak = (correlation_history_[1] == maximum_) &&
                    (correlation_history_[0] < maximum_) &&
                    (maximum_ >= kPeakThreshold);

        if (correlation < 0)
        {
            // Reset the peak detector at each valley in the detection function
            // so that we can detect several consecutive peaks.
            maximum_ = 0;
        }

        if (peak)
        {
            // We can approximate the sub-sample position of the peak by
            // comparing the relative correlation of the samples before and
            // after the raw peak.
            float left = correlation_history_[1] - correlation_history_[2];
            float right = correlation_history_[1] - correlation_history_[0];
            float tilt = 0.5 * (left - right) / (left + right);

            float a = phase_history_[1];
            float b = phase_history_[(tilt < 0) ? 2 : 0];
            float t = Abs(tilt);
            float phase_i = Lerp(Cosine(a), Cosine(b), t);

            // We're trying to resolve a 180-degree phase ambiguity, so we only
            // need to look at the in-phase (real) component to determine
            // whether the phase offset is 0 or 0.5. The quadrature (imaginary)
            // component is irrelevant.
            decision_vector_.Write(phase_i);

            if (++correlation_peaks_ == kNumCorrelationPeaks)
            {
                // Quantize decision phase to 0 or 0.5
                return (decision_vector_.sum() > 0) ? 0 : 0.5;
            }
        }

        return std::nullopt;
    }

    float output(void)
    {
        return correlation_history_[0];
    }
};

}
