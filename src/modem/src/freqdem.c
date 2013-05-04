/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2013 Joseph Gaeddert
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// Frequency modulator/demodulator
//

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "liquid.internal.h"

// freqdem
struct FREQDEM(_s) {
    // common
    float kf;                   // modulation index

    // derived values
    float twopikf_inv;          // 1/(2*pi*kf)
    float dphi;                 // carrier frequency [radians]

    // demodulator
    liquid_freqdem_type type;   // demodulator type (PLL, DELAYCONJ)
    NCO_CRC() oscillator;       // nco (for phase-locked loop demod)
    TC q;                       // phase difference
    FIRFILT_CRC() rxfilter;     // initial receiver filter
    IIRFILT_RRR() postfilter;   // post-filter
};

// create freqdem object
//  _kf     :   modulation factor
//  _type   :   demodulation type (e.g. LIQUID_FREQDEM_DELAYCONJ)
FREQDEM() FREQDEM(_create)(float               _kf,
                           liquid_freqdem_type _type)
{
    // validate input
    if (_kf <= 0.0f || _kf > 1.0) {
        fprintf(stderr,"error: freqdem_create(), modulation factor %12.4e out of range [0,1]\n", _kf);
        exit(1);
    }

    // create main object memory
    FREQDEM() q = (FREQDEM()) malloc(sizeof(struct FREQDEM(_s)));

    // set basic internal properties
    q->type = _type;    // demod type
    q->kf   = _kf;      // modulation factor

    // compute derived values
    q->twopikf_inv = 1.0f / (2*M_PI*q->kf);       // 1 / (2*pi*kf)

    // create oscillator and initialize PLL bandwidth
    q->oscillator = NCO_CRC(_create)(LIQUID_VCO);
    NCO_CRC(_pll_set_bandwidth)(q->oscillator, 0.08f);

    // create initial rx filter
    q->rxfilter = FIRFILT_CRC(_create_kaiser)(17, 0.2f, 40.0f, 0.0f);

    // create DC-blocking post-filter
#if LIQUID_FPM
    q16_t b[2] = {q16_one, -q16_one};
    q16_t a[2] = {q16_one, -q16_one+(q16_one>>6)};
#else
    float b[2] = {1.0f, -1.0f  };
    float a[2] = {1.0f, -0.9999f};
#endif
    q->postfilter = IIRFILT_RRR(_create)(b, 2, a, 2);

    // reset modem object
    FREQDEM(_reset)(q);

    return q;
}

// destroy modem object
void FREQDEM(_destroy)(FREQDEM() _q)
{
    // destroy rx filter
    FIRFILT_CRC(_destroy)(_q->rxfilter);

    // destroy post-filter
    IIRFILT_RRR(_destroy)(_q->postfilter);

    // destroy nco object
    NCO_CRC(_destroy)(_q->oscillator);

    // free main object memory
    free(_q);
}

// print modulation internals
void FREQDEM(_print)(FREQDEM() _q)
{
    printf("freqdem:\n");
    printf("    mod. factor :   %8.4f\n", _q->kf);
}

// reset modem object
void FREQDEM(_reset)(FREQDEM() _q)
{
    // reset oscillator, phase-locked loop
    NCO_CRC(_reset)(_q->oscillator);

    // clear complex phase term
#if LIQUID_FPM
    _q->q.real = 0;
    _q->q.imag = 0;
#else
    _q->q = 0.0f;
#endif
}

// demodulate sample
//  _q      :   FM demodulator object
//  _r      :   received signal
//  _m      :   output message signal
void FREQDEM(_demodulate)(FREQDEM() _q,
                          TC        _r,
                          T *       _m)
{
    // apply rx filter to input
    FIRFILT_CRC(_push)(_q->rxfilter, _r);
    FIRFILT_CRC(_execute)(_q->rxfilter, &_r);

    if (_q->type == LIQUID_FREQDEM_PLL) {
        // 
        // push through phase-locked loop
        //

        // compute phase error from internal NCO complex exponential
        TC p;
        NCO_CRC(_cexpf)(_q->oscillator, &p);
#if LIQUID_FPM
        // TODO: compute actual error
        T phase_error = 0;
#else
        float phase_error = cargf( conjf(p)*_r );
#endif

        // step the PLL and the internal NCO object
        NCO_CRC(_pll_step)(_q->oscillator, phase_error);
        NCO_CRC(_step)(_q->oscillator);

        // demodulated signal is (weighted) nco frequency
        *_m = (NCO_CRC(_get_frequency)(_q->oscillator) -_q->dphi) * _q->twopikf_inv;
    } else {
        // compute phase difference and normalize by modulation index
#if LIQUID_FPM
        // TODO: compute actual output
        *_m = 0;
        _q->q.real = _r.real;
        _q->q.imag = _r.imag;
#else
        *_m = (cargf(conjf(_q->q)*(_r)) - _q->dphi) * _q->twopikf_inv;
        _q->q = _r;
#endif
    }

    // apply post-filtering
    IIRFILT_RRR(_execute)(_q->postfilter, *_m, _m);
}


