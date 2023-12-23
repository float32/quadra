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
#include "util.h"

namespace quadra
{

template <uint32_t symbol_duration>
class CarrierRejectionFilter
{
protected:
    struct Biquad
    {
        float b[3];
        float a[2];
    };

    /* [[[cog
    import crf

    filters = list(crf.generate())
    num_sec = crf.NUM_SECTIONS

    for Tsym, sos in filters:
        cog.outl('static constexpr Biquad kFilter{0:02}[{1}] ='
            .format(Tsym, num_sec))
        cog.outl('{')
        print_coeff = lambda c: '{: .8e},'.format(c).ljust(17)
        for sec in sos:
            cog.outl('    {')
            b = ''.join([print_coeff(c) for c in sec[:3]])
            a = ''.join([print_coeff(c) for c in sec[4:]])
            cog.outl('        {' + b + '},')
            cog.outl('        {' + a + '},')
            cog.outl('    },')
        cog.outl('};')

    cog.outl('')

    cog.outl('static_assert(')
    for Tsym, _ in filters:
        cog.outl('    symbol_duration == {0:>2} ||'.format(Tsym))
    cog.outl('    false, "Unsupported symbol duration");')

    cog.outl('')

    cog.outl('static constexpr const Biquad* kFilter =')
    for Tsym, _ in filters:
        cog.outl('    symbol_duration == {0:>2} ? kFilter{0:02} :'.format(Tsym))
    cog.outl('                            nullptr;')

    cog.outl('')

    cog.outl('static constexpr int32_t kNumSections = {};'.format(num_sec))
    ]]] */
    static constexpr Biquad kFilter05[2] =
    {
        {
            { 3.92776413e-02,  7.85552825e-02,  3.92776413e-02, },
            {-3.79928658e-01,  5.60593774e-02, },
        },
        {
            { 1.00000000e+00,  2.00000000e+00,  1.00000000e+00, },
            {-3.20574398e-01,  2.50042978e-01, },
        },
    };
    static constexpr Biquad kFilter06[2] =
    {
        {
            { 2.22461678e-02,  4.44923356e-02,  2.22461678e-02, },
            {-6.00047253e-01,  1.07855334e-01, },
        },
        {
            { 1.00000000e+00,  2.00000000e+00,  1.00000000e+00, },
            {-5.87365297e-01,  2.88296807e-01, },
        },
    };
    static constexpr Biquad kFilter08[2] =
    {
        {
            { 8.90855348e-03,  1.78171070e-02,  8.90855348e-03, },
            {-8.90333311e-01,  2.12089103e-01, },
        },
        {
            { 1.00000000e+00,  2.00000000e+00,  1.00000000e+00, },
            {-9.30043914e-01,  3.73040930e-01, },
        },
    };
    static constexpr Biquad kFilter10[2] =
    {
        {
            { 4.28742029e-03,  8.57484059e-03,  4.28742029e-03, },
            {-1.07701239e+00,  3.00943042e-01, },
        },
        {
            { 1.00000000e+00,  2.00000000e+00,  1.00000000e+00, },
            {-1.14096126e+00,  4.47300396e-01, },
        },
    };
    static constexpr Biquad kFilter12[2] =
    {
        {
            { 2.32292006e-03,  4.64584012e-03,  2.32292006e-03, },
            {-1.20854549e+00,  3.73931646e-01, },
        },
        {
            { 1.00000000e+00,  2.00000000e+00,  1.00000000e+00, },
            {-1.28361256e+00,  5.08339473e-01, },
        },
    };
    static constexpr Biquad kFilter16[2] =
    {
        {
            { 8.59253439e-04,  1.71850688e-03,  8.59253439e-04, },
            {-1.38286746e+00,  4.84047812e-01, },
        },
        {
            { 1.00000000e+00,  2.00000000e+00,  1.00000000e+00, },
            {-1.46367541e+00,  5.99552135e-01, },
        },
    };

    static_assert(
        symbol_duration ==  5 ||
        symbol_duration ==  6 ||
        symbol_duration ==  8 ||
        symbol_duration == 10 ||
        symbol_duration == 12 ||
        symbol_duration == 16 ||
        false, "Unsupported symbol duration");

    static constexpr const Biquad* kFilter =
        symbol_duration ==  5 ? kFilter05 :
        symbol_duration ==  6 ? kFilter06 :
        symbol_duration ==  8 ? kFilter08 :
        symbol_duration == 10 ? kFilter10 :
        symbol_duration == 12 ? kFilter12 :
        symbol_duration == 16 ? kFilter16 :
                                nullptr;

    static constexpr int32_t kNumSections = 2;
    // [[[end]]]

    Vector x_[kNumSections][3];
    Vector y_[kNumSections][2];

public:
    void Init(void)
    {
        for (int32_t i = 0; i < kNumSections; i++)
        {
            x_[i][0] = 0;
            x_[i][1] = 0;
            x_[i][2] = 0;
            y_[i][0] = 0;
            y_[i][1] = 0;
        }
    }

    Vector Process(Vector in)
    {
        for (int32_t i = 0; i < kNumSections; i++)
        {
            // Shift x state
            x_[i][2] = x_[i][1];
            x_[i][1] = x_[i][0];
            x_[i][0] = in;

            Vector out = 0;

            // Add x state
            out += kFilter[i].b[0] * x_[i][0];
            out += kFilter[i].b[1] * x_[i][1];
            out += kFilter[i].b[2] * x_[i][2];

            // Subtract y state
            out -= kFilter[i].a[0] * y_[i][0];
            out -= kFilter[i].a[1] * y_[i][1];

            // Shift y state
            y_[i][1] = y_[i][0];
            y_[i][0] = out;
            in = out;
        }

        return output();
    }

    Vector output(void)
    {
        return y_[kNumSections - 1][0];
    }
};

}
