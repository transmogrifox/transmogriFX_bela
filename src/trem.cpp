#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "trem.h"


void trem_circuit_preset(trem_coeffs* cf, int ckt )
{
    switch(ckt)
    {
        case MODERATE:
            cf->depth = 0.6;  //approximately +/-3dB
            cf->gain = 1.29;
            cf->lfo_type = RELAX;
            set_lfo_type(cf->lfo, cf->lfo_type);
            update_lfo(cf->lfo, 5, cf->fs);
            break;
        case TREM_SQUARE:
            cf->depth = 1.0;  //deep
            cf->gain = 1.25;
            cf->lfo_type = SQUARE;
            set_lfo_type(cf->lfo, cf->lfo_type);
            update_lfo(cf->lfo, 2.0, cf->fs);
            break;
        default:
            cf->depth = 2.0;  //all the way down
            cf->gain = 2.0; //No mistake it's working
            cf->lfo_type = TRI;
            set_lfo_type(cf->lfo, cf->lfo_type);
            update_lfo(cf->lfo, 3.0, cf->fs);
            break;
    }
}


trem_coeffs*
make_trem(trem_coeffs* cf, float fs)
{
    //First malloc the struct
    cf = (trem_coeffs*) malloc(sizeof(trem_coeffs));
    cf->lfo = init_lfo(cf->lfo, 1.0, fs, 0.0);
    cf->fs = fs;

	//Default to mild mannered tremolo
	trem_circuit_preset( cf, MODERATE );
	cf->bypass = true;

    return cf;
}

inline float
trem_tick(trem_coeffs* cf, float x_)
{
	float out = x_;
    float lfo = cf->gain*(1.0 - cf->depth*run_lfo(cf->lfo));

    return lfo*out;
}

void
trem_tick_n(trem_coeffs* cf, float *x, int n)
{
	if(cf->bypass == true) return;
    for(int i=0; i<n; i++) x[i] = trem_tick(cf, x[i]);
}

void
trem_set_lfo_rate(trem_coeffs* cf, float rate)
{
    update_lfo(cf->lfo, rate, cf->fs);
}

void
trem_set_lfo_depth(trem_coeffs* cf, float depth)
{
    cf->depth = depth;
}

void
trem_set_lfo_gain(trem_coeffs* cf, float gain)
{
    cf->gain = gain;
}

void
trem_set_lfo_type(trem_coeffs* cf, unsigned int type)
{
    cf->lfo_type = type;
    set_lfo_type(cf->lfo, type);
}

bool trem_toggle_bypass(trem_coeffs* cf)
{
	if(cf->bypass == true) cf->bypass = false;
	else cf->bypass = true;
	return cf->bypass;
}
