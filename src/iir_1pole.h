//
// 1-pole IIR filters
//

#ifndef IIR1P_H
#define IIR1P_H

#define LPF1P   0
#define HPF1P   1

typedef struct iir_1p_t
{
    float fs;
    float gain;  //Pass-band gain

    // Coefficients
    float a1;  // Denominator y[n-1]
    float a2;  // Denominator y[n-2]
    float b0;  // Numerator x[n]
    float b1;  // Numerator x[n-1]
    float b2;  // Numerator x[n-2]

    // State variables
    float x1, x2;
    float y1, y2;
} iir_1p;

void compute_filter_coeffs_1p(iir_1p* cf, unsigned int type, float fs, float f0);

// Get frequency response, phase and magnitude
void iir_get_response(iir_1p* cf, float n, float fstart, float fstop, float* frq, float* mag, float* phase);

//
// Execute the 1rst-order IIR difference equation
//

inline float tick_filter_1p(iir_1p* f, float x)
{
    f->y1 = f->b0*x + f->b1*f->x1 + f->a1*f->y1;
    f->x1 = x;
    return f->y1;  // Note gain not implemented here
}

// Use this one if coefficients were computed with gain term
// separated, or if wanting to add gain
inline float tick_filter_1p_g(iir_1p* f, float x)
{
    f->y1 = f->b0*x + f->b1*f->x1 + f->a1*f->y1;
    f->x1 = x;
    return f->y1*f->gain;
}

//
// Execute the 2nd-order IIR difference equation
//

inline float tick_filter_biquad(iir_1p* f, float x)
{
    float yn = f->b0*x + f->b1*f->x1 + f->b2*f->x2 + \
                         f->a1*f->y1 + f->a2*f->y2;
    f->x2 = f->x1;
    f->x1 = x;
    f->y2 = f->y1;
    f->y1 = yn;
    return yn*f->gain;
}

#endif //IIR1P_H
