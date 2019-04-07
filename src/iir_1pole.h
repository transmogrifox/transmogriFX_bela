//
// 1-pole IIR filters
//

#ifndef IIR1P_H
#define IIR1P_H

#define LPF1P   0
#define HPF1P   1

typedef struct iir_1p_t
{
    // Coefficients
    float a1;
    float b0;
    float b1;

    // State variables
    float x1;
    float y1;
} iir_1p;

void compute_filter_coeffs_1p(iir_1p* cf, unsigned int type, float fs, float f0);

//
// Execute the 1rst-order IIR difference equation
//

inline float tick_filter_1p(iir_1p* f, float x)
{
    f->y1 = f->b0*x + f->b1*f->x1 + f->a1*f->y1;
    f->x1 = x;
    return f->y1;
}

#endif //IIR1P_H
