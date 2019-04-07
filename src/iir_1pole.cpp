#include <math.h>
#include "iir_1pole.h"

//
// First order high-pass and low-pass filters.
//

void compute_filter_coeffs_1p(iir_1p* cf, unsigned int type, float fs, float f0)
{
    float w0 = 2.0*M_PI*f0/fs;
    float a1;
    float b0, b1;
    float g = 1.0;  // This could be brought out into a user-configurable param

    switch(type){
    	 default:
         case LPF1P:
            //1-pole high pass filter coefficients
            // H(z) = g * (1 - z^-1)/(1 - a1*z^-1)
            // Direct Form 1:
            //    h[n] = g * ( b0*x[n] - b1*x[n-1] ) - a1*y[n-1]
            // In below implementation gain is redistributed to the numerator:
            //    h[n] = gb0*x[n] - gb1*x[n-1] - a1*y[n-1]
            a1 = -expf(-w0);
            g = (1.0 + a1)/1.12;
            b0 = g;
            b1 = 0.12*g; //0.12 zero improves RC filter emulation at higher freqs.
            break;
         case HPF1P:
            //1-pole high pass filter coefficients
            // H(z) = g * (1 - z^-1)/(1 - a1*z^-1)
            // Direct Form 1:
            //    h[n] = g * ( b0*x[n] -  b1*x[n-1] ) - a1*y[n-1]
            // In below implementation gain is redistributed to the numerator:
            //    h[n] = g*x[n] - g*x[n-1] - a1*y[n-1]
            a1 = -expf(-w0);
            g = (1.0 - a1)*0.5;
            b0 = g;
            b1 = -g;
            break;

    }


    cf->b0 = b0;
    cf->b1 = b1;
    cf->a1 = -a1;  // filter implementation uses addition instead of subtraction

    cf->y1 = 0.0;
    cf->x1 = 0.0;

}


