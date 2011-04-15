/*
 * Copyright (c) 2007, 2008, 2009, 2010 Joseph Gaeddert
 * Copyright (c) 2007, 2008, 2009, 2010 Virginia Polytechnic
 *                                      Institute & State University
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

#include "autotest/autotest.h"
#include "liquid.h"

//
// AUTOTEST: Hamming (12,8) codec
//
void autotest_hamming128_codec()
{
    unsigned int n=4;
    unsigned char msg_org[] = {0x25, 0x62, 0x3F, 0x52};
    fec_scheme fs = LIQUID_FEC_HAMMING128;

    // create arrays
    unsigned int n_enc = fec_get_enc_msg_length(fs,n);
    unsigned char msg_dec[n];
    unsigned char msg_enc[n_enc];

    // create object
    fec q = fec_create(fs,NULL);
    if (liquid_autotest_verbose)
        fec_print(q);

    // encode message
    fec_encode(q, n, msg_org, msg_enc);
    
    // corrupt encoded message
    msg_enc[0] ^= 0x04;
    msg_enc[4] ^= 0x04;

    // decode message
    fec_decode(q, n, msg_enc, msg_dec);

    // validate data are the same
    CONTEND_SAME_DATA(msg_org, msg_dec, n);

    // clean up objects
    fec_destroy(q);
}

