#ifndef READ_VI_TRACE_H
#define READ_VI_TRACE_H

#include <math.h>

typedef struct vi_trace_t
{
    int cnt;
    float minamp, maxamp;
    float di;

    float* volt;
    float* amp;  // Not strictly needed, but might be ueful for
                 // user interface purposes
} vi_trace;

int load_vi_data(vi_trace* vi, char* filename);
void vi_trace_cleanup(vi_trace* vi);

//
// Lagrange polynomial interpolator
// 
inline float vi_trace_interp(vi_trace* vi, float x_)
{
    float x = (x_ - vi->minamp)*vi->di;
    float nx = x;
    float fl = floorf(nx);
    x = x - fl;
    int n = lrintf(fl);

    // Expecting a function that is flatline at each
    // element at each extreme
    if ( n > (vi->cnt - 3) )
    {
        return vi->volt[vi->cnt - 2];
    }
    else if (n < 2)
    {
        return vi->volt[1];
    }
    float p0 = vi->volt[n-1];
    float p1 = vi->volt[n];
    float p2 = vi->volt[n+1];
    float p3 = vi->volt[n+2];

    float c0p0 = -0.16666666667f * p0;
    float c1p1 = 0.5f * p1;
    float c2p2 = -0.5f * p2;
    float c3p3 = 0.16666666667f * p3;

    float a = c3p3 + c2p2 + c1p1 + c0p0;
    float b = -3.0f * c0p0 - p1 - c2p2;
    float c = 2.0f * c0p0 - c1p1 + p2 - c3p3;
    float d = p1;

    x = ((a * x + b) * x + c) * x + d;
    return x;
}

#endif // READ_VI_TRACE_H
