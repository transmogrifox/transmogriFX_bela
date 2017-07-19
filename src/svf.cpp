// svf.cpp
// Implementation of
// Double-samplerate state variable filter with
// linear interpolated over-sampling.
// -----------------------------------------------------------------------------
// This is free and unencumbered software released into the public domain.
//
// For more information, please refer to <http://unlicense.org/>
// terms re-iterated in UNLICENSE.txt
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "svf.h"

sv_filter*
svf_make_filter(sv_filter* s, float fs)
{
    s = (sv_filter*) malloc(sizeof(sv_filter));

    s->fs = 2.0*fs;
    float fc = 700.0;

    svf_set_drive(s, 0.25);

    //s->f = 2.0 * sinf (M_PI * s->fc / s->fs);
    //Very little error for fc < fs/4
    //Reasonably accurate up to fs/2 because of double sample rate processing
    s->fcnst = 2.0 * M_PI / s->fs;
    s->f = svf_compute_f (s, fc);
    svf_set_q(s, 4.0);
    s->g = s->q;
    s->g5 = 0.5 * s->g;
    s->gout = 1.0/s->q;
    s->normalize = true;
    s->outclip = true;

    s->lmix = 1.0;
    s->bmix = 0.0;
    s->hmix = 0.0;

    s->lpf = 0.0;
    s->bpf = 0.0;
    s->hpf = 0.0;

    s->x1 = 0.0;
    s->f1 = s->f;

    return s;
}

inline float
soft_clip(sv_filter* s, float xn_)
{
    float xn = s->drive*xn_;
    if(xn > 1.0) xn = 1.0;
    else if (xn < -1.0) xn = - 1.0;
    else if(xn < 0.0) xn = (xn + 1.0)*(xn + 1.0) - 1.0;
    else xn = 1.0 - (1.0 - xn)*(1.0 - xn);
    return s->idrive*xn;
}

inline float
sqr(float x)
{
    return x*x;
}

inline float
clip1(float x)
{

    float thrs = 0.8;
    float nthrs = -0.72;
    float f=1.25;

    //Hard limiting
    if(x >= 1.2) x = 1.2;
    if(x <= -1.12) x = -1.12;

    //Soft clipping
    if(x > thrs){
        x -= f*sqr(x - thrs);
    }
    if(x < nthrs){
        x += f*sqr(x - nthrs);
    }

    return x;
}

void
svf_tick_n(sv_filter* s, float* x, int N)
{
    int i = 0;

    //double-sample rate processing, linear interpolated
    for(i=0; i < N; i++)
    {
        //Run 1 : Linear interpolate between x[n-1] and x[i]
        s->lpf = s->lpf + s->f* s->bpf;
        s->hpf = s->g5 * (x[i] + s->x1) - s->lpf - s->q*s->bpf;
        s->bpf = s->f* s->hpf + s->bpf;

        //Run 2
        s->lpf = s->lpf + s->f* s->bpf;
        s->hpf = s->g * x[i] - s->lpf - s->q*s->bpf;
        s->bpf = s->f* s->hpf + s->bpf;

        s->x1 = x[i];
        x[i] = s->lmix*s->lpf + s->hmix*s->hpf + s->bmix*s->bpf;
        if(s->normalize == false) x[i] *= s->gout;
    }
}

void
svf_tick_fmod_n(sv_filter* s, float* x, float *f, int N)
{
    int i = 0;

    //double-sample rate processing, linear interpolated
    for(i=0; i < N; i++)
    {
        //Run 1 : Linear interpolate between x[n-1] and x[i]
        s->lpf = s->lpf + 0.5*(f[i] + s->f1)* s->bpf;
        s->hpf = s->g5 * (x[i] + s->x1) - s->lpf - s->q*s->bpf;
        s->bpf = f[i]* s->hpf + s->bpf;

        //Run 2
        s->lpf = s->lpf + f[i]* s->bpf;
        s->hpf = s->g * x[i] - s->lpf - s->q*s->bpf;
        s->bpf = f[i]* s->hpf + s->bpf;

        s->x1 = x[i];
        s->f1 = f[i];
        x[i] = s->lmix*s->lpf + s->hmix*s->hpf + s->bmix*s->bpf;
        if(s->normalize == false) x[i] *= s->gout;
    }
}

void
svf_tick_fmod_soft_clip_n(sv_filter* s, float* x, float *f, int N)
{
    int i = 0;

    //double-sample rate processing, linear interpolated
    for(i=0; i < N; i++)
    {
        //Run 1 : Linear interpolate between x[n-1] and x[i]
        s->lpf = soft_clip(s, s->lpf + 0.5*(f[i] + s->f1)* s->bpf);
        s->hpf = soft_clip(s, s->g5 * (x[i] + s->x1) - s->lpf - s->q*s->bpf);
        s->bpf = soft_clip(s, f[i]* s->hpf + s->bpf);

        //Run 2
        s->lpf = soft_clip(s, s->lpf + f[i]* s->bpf);
        s->hpf = soft_clip(s, s->g * x[i] - s->lpf - s->q*s->bpf);
        s->bpf = soft_clip(s, f[i]* s->hpf + s->bpf);

        s->x1 = x[i];
        s->f1 = f[i];
        x[i] = s->lmix*s->lpf + s->hmix*s->hpf + s->bmix*s->bpf;
        if(s->normalize == false)
            x[i] *= s->gout;
        if(s->outclip)
            x[i] = clip1(x[i]);

    }
}

//Settings
void
svf_set_q(sv_filter* s, float Q)
{
	float qq = Q;
	if(Q < 0.5)
		qq = 0.5;
    s->q = 1.0/qq;
}

float
svf_compute_f (sv_filter* s, float fc)
{
    s->fc = fc;
    return s->fcnst * s->fc;
}

void
svf_set_drive(sv_filter* s, float drive_)
{
    float drive = drive_*drive_;
    if(drive > 2.0) drive = 2.0;
    float idrive = 1.0/(drive + 0.001);
    s->drive = 0.5*(drive + 0.001);
    s->idrive = idrive;
}

void
svf_set_mix_lpf(sv_filter* s, float mix_)
{
    float mix = mix_;
    if(mix > 1.0) mix = 1.0;
    else if (mix < -1.0) mix = -1.0;

    s->lmix = mix;
}

void
svf_set_mix_bpf(sv_filter* s, float mix_)
{
    float mix = mix_;
    if(mix > 1.0) mix = 1.0;
    else if (mix < -1.0) mix = -1.0;

    s->bmix = mix;
}

void
svf_set_mix_hpf(sv_filter* s, float mix_)
{
    float mix = mix_;
    if(mix > 1.0) mix = 1.0;
    else if (mix < -1.0) mix = -1.0;

    s->hmix = mix;
}

void
svf_set_normalize(sv_filter* s, bool n)
{
    s->normalize = n;
}

void
svf_set_outclip(sv_filter* s, bool clip_output)
{
    s->outclip = clip_output;
}
