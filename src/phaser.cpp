#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "phaser.h"

void commit_circuit_config(phaser_coeffs* cf)
{
    int i = 0;
    //There is always something to calculate for position zero
	cf->a1p_min[i] = -expf(-(cf->w_min[i])/cf->fs);
	cf->a1p_max[i] = -expf(-(cf->w_max[i])/cf->fs);
	cf->a1p_dif[i] = cf->a1p_max[i] - cf->a1p_min[i];
	cf->a1p[i] = cf->a1p_min[i];
	cf->ghpf[i] = (1.0 - cf->a1p[i])*0.5;
	
    for(i = 1; i < cf->n_stages; i++)
    {
    	if(cf->stagger[i]) {
	        cf->a1p_min[i] = -expf(-(cf->w_min[i])/cf->fs);
	        cf->a1p_max[i] = -expf(-(cf->w_max[i])/cf->fs);
	        cf->a1p_dif[i] = cf->a1p_max[i] - cf->a1p_min[i];
	        cf->a1p[i] = cf->a1p_min[i];
	        cf->ghpf[i] = (1.0 - cf->a1p[i])*0.5;
    	} else
    	{
    		//Should never use these but it's a safe default
    		//in the event some unforeseen conditions exists in which 
    		//stagger is set true for what would be an unitialized set of 
    		//coefficients
	        cf->a1p_min[i] = cf->a1p_min[0];
	        cf->a1p_max[i] = cf->a1p_max[0];
	        cf->a1p_dif[i] = cf->a1p_dif[0];
	        cf->a1p[i] = cf->a1p[0];
	        cf->ghpf[i] = cf->ghpf[0];    		
    	}
    }

}

void phaser_circuit_preset(int ckt, phaser_coeffs* cf)
{
    int i = 0;
	switch(ckt)
	{
		case PHASE_90:
            cf->n_stages = 4;
            for(i = 0; i < PHASER_MAX_STAGES; i++)
            {
                cf->gfb[i] = 0.0;  //feedback gains all 0
                cf->w_min[i] = 2.0*M_PI*350.0;
                cf->w_max[i] = 2.0*M_PI*1800.0;
                cf-> w_diff[i] = cf->w_max[i] - cf->w_min[i];
                cf->stagger[i] = false;
                cf->apply_feedback[i] = false;
            }
            cf->gfb[3] = 0.455; //tap feedback from stage 4
            cf->apply_feedback[3] = true;
            cf->distort = 0.5;
            cf->idistort = 1.0/cf->distort;
            cf->wet = 1.0;
            cf->dry = 1.0;
            cf->mod->lfo_type = RELAX;
			break;
		case PHASER_DEFAULT:
        default:
            cf->n_stages = 6;
            for(i = 0; i < PHASER_MAX_STAGES; i++)
            {
                cf->gfb[i] = 0.0;  //feedback gains all 0
                cf->w_min[i] = 2.0*M_PI*350.0;
                cf->w_max[i] = 2.0*M_PI*2000.0;
                cf-> w_diff[i] = cf->w_max[i] - cf->w_min[i];
                cf->stagger[i] = false;
                cf->apply_feedback[i] = false;
            }
            cf->distort = 0.5;
            cf->idistort = 1.0/cf->distort;
            cf->wet = 1.0;
            cf->dry = 1.0;
            cf->mod->lfo_type = EXP;
		break;

	}

		//Compute DSP filter coefficients
	commit_circuit_config(cf);
}

//Initialize filter state variables
void zero_state_variables(phaser_coeffs* cf)
{
    cf->fb = 0.0;
    int i = 0;
    for(i = 0; i < PHASER_MAX_STAGES; i++)
    {
        cf->yh1[i] = 0.0;
        cf->xh1[i] = 0.0;
        cf->ph1[i] = 0.0;
    }
    
    //Signal this has already been done so 
    cf->reset = false;

}

phaser_coeffs*
make_phaser(phaser_coeffs* cf, float fs)
{
    //First malloc the struct
    cf = (phaser_coeffs*) malloc(sizeof(phaser_coeffs));
    cf->fs = fs;
	//setup LFO
	cf->mod = init_lfo(cf->mod,1.0, cf->fs, 0.0);
	
	//Default to something that makes decent sound
	phaser_circuit_preset(PHASER_DEFAULT, cf);

	//Initialize everything to 0.0
	zero_state_variables(cf);
	
	//Start out bypassed
    cf->bypass = true;

    return cf;
}

inline float sqr(float x)
{
    return x*x;
}

inline float clip_hard(float x)
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

inline void
phaser_modulate(phaser_coeffs* cf)
{
    unsigned int st = 0;
    float lfo = 1.0 - run_lfo(cf->mod);
    lfo += lfo*lfo;
    lfo *= 0.5;
    
    cf->a1p[st] = cf->a1p_min[st] + cf->a1p_dif[st]*lfo;
    cf->ghpf[st] = (1.0 - cf->a1p[st])*0.5;
    if(cf->stagger[0] == false) return;
    
    for(st = 1; st < cf->n_stages; st++)
    {
    	if(cf->stagger[st]) 
    	{
	        cf->a1p[st] = cf->a1p_min[st] + cf->a1p_dif[st]*lfo;
	        cf->ghpf[st] = (1.0 - cf->a1p[st])*0.5;
    	}
    }
}

inline float
phaser_tick(float x_, phaser_coeffs* cf)
{
    unsigned int st = 0;
    unsigned int stn = 0;
    unsigned int stag = 0;

    phaser_modulate(cf);
    cf->ph1[0] = x_ + cf->fb;
    cf->fb = 0.0;
    for(st = 0; st < cf->n_stages; st++)
    {
        if(cf->stagger[st])
        {
            stag = st;
        } else
        {
            stag = 0;
        }
        if(st > 0) stn = st -1;
        cf->yh1[st] = cf->ghpf[stag] * ( cf->ph1[stn] - cf->xh1[st] ) - cf->a1p[stag]*cf->yh1[st];
        cf->xh1[st] = cf->ph1[stn];
        cf->ph1[st] = 2.0*cf->idistort*clip_hard(cf->distort*cf->yh1[st]) - cf->ph1[stn];
        //cf->ph1[st] = 2.0*cf->yh1[st] - cf->ph1[stn];
        if(cf->apply_feedback[st])
        {
            cf->fb += cf->ph1[st]*cf->gfb[st];
        }

    }

    float out = cf->wet*cf->ph1[cf->n_stages-1] + cf->dry*x_;

    //And ship it!
    return out;

}

void
phaser_tick_n(phaser_coeffs* cf, int n, float* x)
{
	if(cf->bypass)
	{
		if(cf->reset)
		{
			zero_state_variables(cf);
		}
		return;
	} else
	{
		cf->reset = true;
	}
	int i = 0;
	for(i=0; i<n; i++)
	{
		x[i] = phaser_tick(x[i], cf);
	}
	
}

void
phaser_set_nstages(phaser_coeffs* cf, int nstages)
{
	//TODO: Check feedback not coming from invalid stage
	cf->n_stages = nstages;
	commit_circuit_config(cf);
}

void
phaser_set_mix(phaser_coeffs* cf, float wet)
{
	float wet_ = wet;
    float gain = 1.0 + 2.0*fabs(wet_);
    if(gain > 2.0) gain = 4.0 - gain;
	
	if(wet < 0.0)
	{
		cf->wet = wet*gain;
		cf->dry = (1.0 + wet)*gain;
	}
	else 
	{
		cf->wet = wet*gain;
		cf->dry = (1.0 - wet)*gain;
	}
}

void 
phaser_set_lfo_type(phaser_coeffs* cf, int n)
{
	cf->mod->lfo_type = n;
}

void 
phaser_set_lfo_rate(phaser_coeffs* cf, float rate)
{
	update_lfo(cf->mod, rate, cf->fs);
}

void
phaser_set_lfo_depth(phaser_coeffs* cf, float depth, int stage)
{
	cf->w_min[stage] = 2.0*M_PI*depth;
	cf->w_max[stage] = cf->w_min[stage] + cf-> w_diff[stage];
	commit_circuit_config(cf);
}

void
phaser_set_lfo_width(phaser_coeffs* cf, float width, int stage)
{
	cf->w_diff[stage] = 2.0*M_PI*width;
	cf->w_max[stage] = cf->w_min[stage] + cf-> w_diff[stage];
	commit_circuit_config(cf);
}

void
phaser_set_feedback(phaser_coeffs* cf, float fb, int stage)
{
	cf->gfb[stage] = fb;
	
	if(fb == 0.0)
		cf->apply_feedback[stage] = false;
	else
		cf->apply_feedback[stage] = true;
}

void
phaser_set_distortion(phaser_coeffs* cf, float d)
{
	if(d < 0.001) 
		cf->distort = 0.001;
	else if (d >1000.0)
		cf->distort = 1000.0;
	else
		cf->distort = d;
		
	cf->idistort = 1.0/d;
}

bool phaser_toggle_bypass(phaser_coeffs* cf)
{
	if(cf->bypass) cf->bypass = false;
	else cf->bypass = true;
	
	return cf->bypass;
}
