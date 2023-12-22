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
#include <complex>
#include "carrier_rejection_filter.h"
#include "correlator.h"
#include "one_pole.h"
#include "pll.h"
#include "util.h"
#include "window.h"

namespace quadra
{

template <uint32_t sample_rate, uint32_t symbol_rate>
class Demodulator
{
public:
    void Init(void)
    {
        state_ = STATE_WAIT_TO_SETTLE;

        hpf_.Init(0.001);
        follower_.Init(0.0001);
        agc_gain_ = 1;

        pll_.Init(1.0 / kSymbolDuration);
        crf_.Init();

        correlator_.Init();

        v_history_.Init();

        decision_phase_ = 0;
        skipped_samples_ = 0;
        carrier_sync_count_ = 0;

        decide_ = false;
    }

    void Reset(void)
    {
        Init();
    }

    void BeginCarrierSync(void)
    {
        state_ = STATE_CARRIER_SYNC;
        carrier_sync_count_ = 0;
    }

    bool Process(uint8_t& symbol, float sample)
    {
        sample = hpf_.Process(sample);

        float env = Abs(sample);

        follower_.Process(env);
        float level = signal_power();
        sample *= agc_gain_;

        if (state_ == STATE_WAIT_TO_SETTLE)
        {
            if (skipped_samples_ < kSettlingTime)
            {
                skipped_samples_++;
            }
            else if (level > kLevelThreshold)
            {
                skipped_samples_ = 0;
                state_ = STATE_SENSE_GAIN;
            }
        }
        else if (state_ == STATE_SENSE_GAIN)
        {
            if (skipped_samples_ < kSettlingTime)
            {
                skipped_samples_++;
            }
            else if (level > kLevelThreshold)
            {
                constexpr float kTwoOverPi = 0.64;
                constexpr float kSqrt2 = 1.41;
                agc_gain_ = kTwoOverPi / level * kIQAmplitude * kSqrt2;
                BeginCarrierSync();
            }
            else
            {
                state_ = STATE_WAIT_TO_SETTLE;
            }
        }
        else if (state_ != STATE_ERROR)
        {
            if (level < kLevelThreshold)
            {
                state_ = STATE_ERROR;
            }
            else
            {
                return Demodulate(symbol, sample);
            }
        }

        return false;
    }

    bool error(void)
    {
        return state_ == STATE_ERROR;
    }

    // Accessors for debug and simulation
    uint32_t state(void)          {return state_;}
    float    pll_phase(void)      {return pll_.phase();}
    float    pll_error(void)      {return pll_.error();}
    float    pll_step(void)       {return pll_.step();}
    float    decision_phase(void) {return decision_phase_;}
    float    signal_power(void)   {return follower_.output();}
    float    recovered_i(void)    {return crf_.output().real();}
    float    recovered_q(void)    {return crf_.output().imag();}
    float    correlation(void)    {return correlator_.output();}
    bool     decide(void)         {return decide_;}
    float    agc(void)            {return agc_gain_;}

protected:
    static constexpr uint32_t kSettlingTime = sample_rate * 0.25;
    static constexpr float kLevelThreshold = 0.05;
    static constexpr uint32_t kCarrierSyncLength = symbol_rate * 0.025;
    static_assert(sample_rate % symbol_rate == 0);
    static constexpr uint32_t kSymbolDuration = sample_rate / symbol_rate;

    enum State
    {
        STATE_WAIT_TO_SETTLE,
        STATE_SENSE_GAIN,
        STATE_CARRIER_SYNC,
        STATE_CARRIER_LOCK,
        STATE_ALIGN,
        STATE_OK,
        STATE_ERROR,
    };

    State state_;

    OnePoleHighpass hpf_;
    OnePoleLowpass follower_;
    float agc_gain_;

    PhaseLockedLoop pll_;
    CarrierRejectionFilter<kSymbolDuration> crf_;

    Correlator correlator_;
    Window<Vector, kSymbolDuration> v_history_;

    float decision_phase_;
    uint32_t skipped_samples_;
    uint32_t carrier_sync_count_;

    bool decide_;

    static constexpr float kAGCSlow = 50e-6;
    static constexpr float kAGCFast = 1e-3;

    void AGCProcess(Vector v, Vector v_bar, float speed)
    {
        float i = v.real();
        float q = v.imag();
        float i_bar = v_bar.real();
        float q_bar = v_bar.imag();
        float error = (i * i + q * q) - (i_bar * i_bar + q_bar * q_bar);
        agc_gain_ -= speed * error;
    }

    bool Demodulate(uint8_t& symbol, float sample)
    {
        float phi = pll_.phase();
        Vector osc{Cosine(phi), -Sine(phi)};
        Vector v = crf_.Process(2 * sample * osc);
        Vector v_bar = Quantize(v);
        v_history_.Write(v);
        decide_ = false;
        bool symbol_valid = false;

        if (state_ == STATE_CARRIER_SYNC)
        {
            pll_.ProcessError(CrossProduct(v, kCarrierSyncVector));
            auto decision = pll_.phase_trigger(0);

            if (decision.has_value())
            {
                decide_ = true;
                symbol = DecideSymbol(*decision);

                if (symbol == kCarrierSyncSymbol)
                {
                    AGCProcess(v, kCarrierSyncVector, kAGCFast);

                    if (++carrier_sync_count_ == kCarrierSyncLength)
                    {
                        state_ = STATE_CARRIER_LOCK;
                        correlator_.Reset();
                    }
                }
                else
                {
                    carrier_sync_count_ = 0;
                }
            }
        }
        else if (state_ == STATE_CARRIER_LOCK)
        {
            pll_.ProcessError(CrossProduct(v, v_bar));
            auto decision0 = pll_.phase_trigger(0);
            auto decision1 = pll_.phase_trigger(0.5);

            if (decision0.has_value() || decision1.has_value())
            {
                decide_ = true;
                float decision = decision0.value_or(*decision1);
                symbol = DecideSymbol(decision);

                AGCProcess(v, kCarrierSyncVector, kAGCFast);
                correlator_.Push(phi, v);

                if (symbol != kCarrierSyncSymbol)
                {
                    state_ = STATE_ALIGN;
                    decision_phase_ = 0;
                }
            }
        }
        else if (state_ == STATE_ALIGN)
        {
            pll_.ProcessError(CrossProduct(v, v_bar));
            auto decision0 = pll_.phase_trigger(0);
            auto decision1 = pll_.phase_trigger(0.5);

            if (decision0.has_value() || decision1.has_value())
            {
                decide_ = true;
                float decision = decision0.value_or(*decision1);
                v = SampleSymbol(decision);
                auto decision_phase = correlator_.Process(phi, v);

                if (decision_phase.has_value())
                {
                    decision_phase_ = *decision_phase;
                    state_ = STATE_OK;
                }
            }
        }
        else if (state_ == STATE_OK)
        {
            float phase_error = CrossProduct(v, v_bar);
            // Raised-cosine weighting to reject noisy error between symbols
            phase_error *= 0.5 * (1 + Cosine(phi - decision_phase_));
            pll_.ProcessError(phase_error);
            auto decision = pll_.phase_trigger(decision_phase_);

            if (decision.has_value())
            {
                decide_ = true;
                symbol = DecideSymbol(*decision);
                symbol_valid = true;
                AGCProcess(v, v_bar, kAGCSlow);
            }
        }

        pll_.Step();
        return symbol_valid;
    }

    static constexpr uint32_t kNumQuanta = 4;
    static constexpr float kIQAmplitude = 1 - 1.0 / kNumQuanta;
    static constexpr Vector kCarrierSyncVector{-kIQAmplitude, -kIQAmplitude};
    static constexpr uint8_t kCarrierSyncSymbol = 0xF;

    int32_t DecisionIndex(float sample)
    {
        sample = (kNumQuanta / 2.0) * (sample + 1);
        return Clamp<int32_t>(sample, 0, kNumQuanta - 1);
    }

    float Quantize(float sample)
    {
        int32_t index = DecisionIndex(sample);
        return kIQAmplitude * (2.0 * index / (kNumQuanta - 1) - 1);
    }

    Vector Quantize(Vector v)
    {
        return {Quantize(v.real()), Quantize(v.imag())};
    }

    float CrossProduct(Vector v1, Vector v2)
    {
        return v1.real() * v2.imag() - v2.real() * v1.imag();
    }

    Vector SampleSymbol(float fractional_delay)
    {
        fractional_delay =
            Clamp<float>(fractional_delay, 0, kSymbolDuration - 1.001);
        int32_t i_late = fractional_delay;
        int32_t i_early = i_late + 1;
        Vector early = v_history_[i_early];
        Vector late = v_history_[i_late];
        return Lerp(late, early, FractionalPart(fractional_delay));
    }

    uint8_t DecideSymbol(float fractional_delay = 0)
    {
        Vector v = SampleSymbol(fractional_delay);
        int32_t i_index = DecisionIndex(v.real());
        int32_t q_index = DecisionIndex(v.imag());

        constexpr uint8_t kIQtoSymbol[4][4] =
        {
            {0xF, 0xD, 0x9, 0xB},
            {0xE, 0xC, 0x8, 0xA},
            {0x6, 0x4, 0x0, 0x2},
            {0x7, 0x5, 0x1, 0x3},
        };

        return kIQtoSymbol[i_index][q_index];
    }
};

}
