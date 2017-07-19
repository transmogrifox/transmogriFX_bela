//
//   Sample/hold modulator program
//
// -----------------------------------------------------------------------------
// This is free and unencumbered software released into the public domain.
//
// For more information, please refer to <http://unlicense.org/>
// terms re-iterated in UNLICENSE.txt
// -----------------------------------------------------------------------------
//

#include	<stdlib.h>

#include	"sample_hold_modulator.h"

inline float
sqr(float x)
{
    return x*x;
}

inline float
soft_clip(float xn_)
{

    float xn = 2.0*xn_ - 1.0;
    if(xn > 1.0) xn = 1.0;
    else if (xn < -1.0) xn = - 1.0;
    else if(xn < 0.0) xn = sqr(xn + 1.0) - 1.0;
    else xn = 1.0 - sqr(1.0 - xn);
    return 0.5*(xn + 1.0);
}

sh_mod*
make_sample_hold(sh_mod* sh, float fs, int N)
{
    sh = (sh_mod*) malloc(sizeof(sh_mod));
    sh->ad = make_adsr(sh->ad, fs, N);
    sh->max_types = SH_MAX_TYPES;
    sh->adsr_env =  (float*) malloc(sizeof(float)*N);
    sh->lfo = (float*) malloc(sizeof(float)*N);
    sh->hfo = (float*) malloc(sizeof(float)*N);
    sh->swave = (float*) malloc(sizeof(float)*N);
    sh->sequence = (float*) malloc(sizeof(float)*MAX_SEQ_STEPS);
    sh->sclk = (bool*) malloc(sizeof(bool)*N);

    for(int i = 0; i < N; i++)
    {
    	sh->adsr_env[i] = 0.0;
        sh->lfo[i] = 0.0;
        sh->hfo[i] = 0.0;
        sh->swave[i] = 0.0;
        sh->sclk[i] = false;
    }

    sh->fs = fs;
    sh->ifs = 1.0/sh->fs;
    sh->NS = N;

    float rate = 6.0;
    float hfr  = 4.7*rate;
    float Trate = 1000.0/rate;

    //Initialize ramp rates
    adsr_set_attack(sh->ad, Trate/8.0);
    adsr_set_decay(sh->ad, Trate/8.0);
    adsr_set_sustain(sh->ad, 0.36);
    adsr_set_release(sh->ad, Trate/4.0);
    adsr_set_trigger_timeout(sh->ad, 0.5/rate);
    sh->en_adsr = true;

    //Initialize ramp rates
    sh->dt_lfo = sh->ifs*rate;
    sh->dt_hfo = sh->ifs*hfr;
    sh->steepness = 3.0;  //how abruptly it transitions from one step to the next
    sh->mode = SH_RAND;
    sh->n_seq_steps = 6;
    for(int i=0; i < sh->n_seq_steps; i++)
    {
        sh->sequence[i] = ((float) rand())/((float) RAND_MAX);
        //sh->sequence[i] = ((float) i)/((float) (sh->n_seq_steps -1) );
    }
    //Initialize state variables
    sh->seq_step = 0;
    sh->ramp_lfo = 0.0;
    sh->ramp_hfo = 0.0;
    sh->last_hfo_sample = 0.0;
    sh->last_wave = 0.0;
    sh->dl = 0.0;

    return sh;
}

void
run_modulator(sh_mod* sh)
{
    for(int i = 0; i < sh->NS; i++)
    {
        sh->ramp_lfo += sh->dt_lfo;
        if(sh->mode == SH_RAND)
        {
           	sh->ramp_hfo = ((float) rand())/((float) RAND_MAX);

        }
        else if (sh->mode == SH_RAMP)
            sh->ramp_hfo += sh->dt_hfo;
        else if (sh->mode == SH_SEQ)
            sh->ramp_hfo = sh->sequence[sh->seq_step];

        if(sh->ramp_lfo >= 1.0)
        {
            sh->ramp_lfo = 0.0;
            sh->sclk[i] = true;
            sh->seq_step += 1;  //go to next value sequence
            if(sh->seq_step >= sh->n_seq_steps) sh->seq_step = 0; //reset sequencer
        }
        else
        {
            sh->sclk[i] = false;
        }

        if(sh->ramp_hfo > 1.0)
        {
            sh->ramp_hfo = 1.0;
        }
        else if (sh->ramp_hfo < 0.0)
        {
            sh->ramp_hfo = 0.0;
        }
        
        sh->lfo[i] = soft_clip(sh->steepness*sh->ramp_lfo);
        sh->hfo[i] = sh->ramp_hfo;
    }
}

void
run_sample_hold(sh_mod* sh, float* output)
{
    run_modulator(sh);
    adsr_tick_n(sh->ad, sh->adsr_env);
    for(int i = 0; i < sh->NS; i++)
    {
        
        if(sh->sclk[i] == true)
        {
            sh->dl = (sh->hfo[i] - sh->last_hfo_sample);
            sh->last_wave = sh->last_hfo_sample;
            sh->last_hfo_sample = sh->hfo[i];
        	adsr_set_trigger_state(sh->ad, true);
        	adsr_set_amplitude(sh->ad, sh->last_hfo_sample);
        }
        sh->swave[i] = sh->dl*sh->lfo[i] + sh->last_wave;
        if(sh->en_adsr)
        	output[i] = sh->adsr_env[i];
        	//output[i] = sh->swave[i]*sh->adsr_env[i];
            //output[i] = sh->last_hfo_sample*sh->adsr_env[i];
            //output[i] = sh->last_hfo_sample;
        else
            output[i] = sh->swave[i];
    }
}

bool
sample_hold_set_active(sh_mod* sh, bool act)
{
	//toggle state if act is true
	if(act)
	{
		if(sh->en_adsr)
			sh->en_adsr = false;
		else
			sh->en_adsr = true;
	} else //always deactivate if false
	{
		sh->en_adsr = false;
	}
	//return the state evaluated
	return sh->en_adsr;
}

void
sample_hold_set_rate(sh_mod* sh, float rate)
{
    float hfr  = 4.7*rate;
    float Trate = 1000.0/rate;

    //Initialize ramp rates
    adsr_set_attack(sh->ad, Trate/10.0);
    adsr_set_decay(sh->ad, Trate/20.0);
    adsr_set_sustain(sh->ad, 0.75);
    adsr_set_release(sh->ad, Trate/16.0);
    adsr_set_trigger_timeout(sh->ad, 0.5/rate);
    sh->en_adsr = true;

    //Initialize ramp rates
    sh->dt_lfo = sh->ifs*rate;
    sh->dt_hfo = sh->ifs*hfr;
}

void
sample_hold_set_type(sh_mod* sh, int type)
{
	int t = type;
	if(t >= SH_MAX_TYPES)
		t = SH_MAX_TYPES - 1;
	else if(t < 0)
		t = 0;
	sh->mode = type;
}