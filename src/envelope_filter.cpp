//
// Envelope filter based on state variable filter
// -----------------------------------------------------------------------------
// This is free and unencumbered software released into the public domain.
//
// For more information, please refer to <http://unlicense.org/>
// terms re-iterated in UNLICENSE.txt
// -----------------------------------------------------------------------------
//

#include <stdlib.h>
#include <math.h>

#include "svf.h"
#include "envelope_filter.h"
#include "sample_hold_modulator.h"

env_filter*
envf_make_filter(env_filter* envf, float fs, int N)
{
    envf = (env_filter*) malloc(sizeof(env_filter));
    envf->svf = svf_make_filter(envf->svf, fs);
    envf->frq = (float*) malloc(sizeof(float)*N);
    envf->y = (float*) malloc(sizeof(float)*N);
    envf->sh_buff = (float*) malloc(sizeof(float)*N);
    envf->sh = make_sample_hold(envf->sh, fs, N);
    envf->shmix = 0.0;
    envf->ishmix = 1.0 - envf->shmix;
    envf->bypass = true;
    
    for(int i=0; i<N; i++) 
    {
    	envf->frq[i] = 0.5;
    	envf->y[i] = 0.0;
    	envf->sh_buff[i] = 0.0;
    }

    envf->N = N;
    envf->fs = fs;
    envf->ts = 1.0/fs;
    envf->nyquist = svf_compute_f(envf->svf, envf->fs/2.0);

    envf_set_atk(envf, 0.01);
    envf_set_rls(envf, 0.15);
	envf_set_gate(envf, -42.0);
	envf->knee_db = 6.0;
	envf_set_gate_knee(envf, envf->knee_db);
    envf->yn = 0.0;

    float fl = 50.0;
    float fh = 4000.0;
    float fnl = svf_compute_f(envf->svf, fl);
    float fnh = svf_compute_f(envf->svf, fh);
    envf->width = fnh - fnl;
    envf->depth = fnl;
    envf->fl = fnl;
    envf->fh = fnh;
    envf->sns = 1.0;
    envf->mix_wet = 1.0;
    envf->mix_dry = 1.0 - envf->mix_wet;

    svf_set_q(envf->svf, 12.0);
    svf_set_drive(envf->svf, 0.125);
    svf_set_normalize(envf->svf, false);
    
    return envf;
}

inline float
soft_clip(float xn_)
{
    float xn = xn_;
    if(xn > 1.0) xn = 1.0;
    else if (xn < -1.0) xn = -1.0;
    else if(xn < 0.0) xn = (xn + 1.0)*(xn + 1.0) - 1.0;
    else xn = 1.0 - (1.0 - xn)*(1.0 - xn);
    return xn;
}

inline float
sqr(float x)
{
    return x*x;
}

inline float
clamp(float x)
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

inline float
soft_gate(float xn_)
{

    float xn = xn_ - 1.0;
    if(xn > 1.0) xn = 1.0;
    else if (xn < -1.0) xn = - 1.0;
    else if(xn < 0.0) xn = sqr(xn + 1.0) - 1.0;
    else xn = 1.0 - sqr(1.0 - xn);
    return 0.5*(xn + 1.0);
}

//Processing functions
void
envf_tick_n(env_filter* envf, float* x, float* e)
{
    float ei = 0.0;
    float a = 0.0;
    
    if(envf->bypass)
    	return;
    
    //TODO: move to env_filter struct
    run_sample_hold(envf->sh, envf->sh_buff);
    
    //First run envelope attack/release
    for(int i=0; i<envf->N; i++)
    {
    	ei = e[i];
    	
    	//Apply gate to envelope detector
        a = fabs(ei) - envf->thrs;
        if(a > 0.0)
        {
            ei *= soft_gate(envf->knee*a);
        }
        
        //Do attack/release processing
        if(ei > envf->yn)
            ei = envf->yn*envf->atk_b + ei*envf->atk_a;
        else
            ei = envf->yn*envf->rls_b + ei*envf->rls_a;
        envf->yn = ei;
        
		//limit to range 0 to 1 and apply to filter frequency setting
		ei = clamp(ei*envf->sns*envf->ishmix + envf->sh_buff[i]*envf->shmix);
        envf->frq[i] = envf->depth + envf->width*ei;
        
        //DEBUG:  overwrite envelope with detector output so 
        //modulation waveform can be viewed in scope from render()
        //e[i] = envf->sh_buff[i];
        
        //copy input over to processing buffer
        envf->y[i] = x[i];
    }

    //Run the state variable filter
    svf_tick_fmod_soft_clip_n(envf->svf, envf->y, envf->frq, envf->N);
    
    //Post processing
    for(int i=0; i<envf->N; i++)
    {
		x[i] = envf->mix_dry * x[i] + envf->mix_wet*envf->y[i];
    }

}

//settings
bool
envf_toggle_bypass(env_filter* envf)
{
	if(envf->bypass)
		envf->bypass = false;
	else
		envf->bypass = true;
	
	return envf->bypass;
		
}

void 
envf_set_bypass(env_filter* envf, bool bp)
{
	envf->bypass = bp;
}

void
envf_set_q(env_filter* envf, float Q)
{
    svf_set_q(envf->svf, Q);
}

void
envf_set_drive(env_filter* envf, float drive_)
{
    svf_set_drive(envf->svf, drive_);
}

void 
envf_set_mix(env_filter* envf, float mix_)
{
	float mix = mix_;
	if(mix > 1.0) mix = 1.0;
	else if (mix < -1.0) mix = -1.0;
	
	envf->mix_wet = mix;
	if(mix > 0.0)
		envf->mix_dry = 1.0 - mix;
	else
		envf->mix_dry = 1.0 + mix;
}

void
envf_set_mix_lpf(env_filter* envf, float mix_)
{
    svf_set_mix_lpf(envf->svf, mix_);
}

void
envf_set_mix_bpf(env_filter* envf, float mix_)
{
    svf_set_mix_bpf(envf->svf, mix_);
}

void
envf_set_mix_hpf(env_filter* envf, float mix_)
{
    svf_set_mix_hpf(envf->svf, mix_);
}

void
envf_set_normalize(env_filter* envf, bool n)
{
    svf_set_normalize(envf->svf, n);
}

void
envf_set_outclip(env_filter* envf, bool clip_output)
{
    svf_set_outclip(envf->svf, clip_output);
}

void
envf_set_atk(env_filter* envf, float t)
{

    envf->atk_a = envf->ts/(envf->ts + t);
    envf->atk_b = 1.0 - envf->atk_a;

}

void
envf_set_rls(env_filter* envf, float t)
{

    envf->rls_a = envf->ts/(envf->ts + t);
    envf->rls_b = 1.0 - envf->rls_a;

}

void
envf_set_sensitivity(env_filter* envf, float sns_)
{
    float sns = sns_;
    if(sns > 10.0f) sns = 10.0f;
    else if (sns < -10.0f) sns = -10.0f;
    envf->sns = sns;

    if(sns < 0.0)
        envf->depth = envf->fh;
    else
        envf->depth = envf->fl;

}

void
envf_set_lfo_rate(env_filter* envf, float r_)
{
	sample_hold_set_rate(envf->sh, r_);
}

void
envf_set_depth(env_filter* envf, float d_)
{
    float d = d_;
    if(d < 0.0) d *= -1.0;
    float fnl = svf_compute_f(envf->svf, d);
    float fnh = fnl + envf->width;
    if(fnh > envf->nyquist) fnh = envf->nyquist;
    if(fnl > envf->nyquist) fnl = envf->nyquist;
    envf->width = fnh - fnl;
    envf->fl = fnl;
    envf->fh = fnh;

    if(envf->sns < 0.0)
        envf->depth = envf->fh;
    else
        envf->depth = envf->fl;
}

void
envf_set_lfo_width(env_filter* envf, float w_)
{
	
}

void
envf_set_width(env_filter* envf, float w_)
{
    float w = w_;
    if(w < 0.0) w *= -1.0;
    float fnl = envf->fl;
    float fnh = envf->fl + svf_compute_f(envf->svf, w);
    if(fnh > envf->nyquist) fnh = envf->nyquist;
    if(fnl > envf->nyquist) fnl = envf->nyquist;
    envf->width = fnh - fnl;
    envf->fl = fnl;
    envf->fh = fnh;

    if(envf->sns < 0.0)
        envf->depth = envf->fh;
    else
        envf->depth = envf->fl;
}

//Functions for gating of the envelope detector
float compute_thrsh(float db)
{
    if(db > 0.0) db = 0.0;
    return powf(10.0, db/20.0);
}

float compute_knee(float t, float db)
{
    float d = db;
    if(d < 0.1) d = 0.1;
    float r = powf(10.0, d/20.0);
    return (1.0/t)*2.0/(r - 1.0);
}

//Set envelope detector gate in dB: -infy:0.0;
//  For practical purposes it probably won't ever be set 
//  higher than -12 dB
void
envf_set_gate(env_filter* envf, float thrs_)
{
	float thrs = thrs_;
	if(thrs > 0.0) thrs = 0.0;
	envf->thrs = powf(10.0, thrs/20.0);
	envf->knee = compute_knee(envf->thrs, envf->knee_db);
}

// Sets how sudden the gate engages.  
//    High number gives slower transition
//    Lower number gives more sudden transition
//  Range is 0.1 dB to 12.0 dB
void
envf_set_gate_knee(env_filter* envf, float knee_)
{
	float knee = knee_;
	if(knee < 0.1) knee = 0.1;
	else if(knee > 12.0) knee = 12.0;
	envf->knee_db = knee;
	
	//transition over range of threshold to threshold + knee
	envf->knee = compute_knee(envf->thrs, knee);
}

//SAMPLE/HOLD AND ADSR
void
envf_set_mix_sh_modulator(env_filter* envf, float mix_)
{
	float mix = mix_;
	if(mix > 1.0) 
		mix = 1.0;
	else if (mix < 0.0)
		mix = 0.0;
	
	envf->shmix = mix;
	envf->ishmix = 1.0 - envf->shmix;

}

void
envf_set_sample_hold_type(env_filter* envf, int type)
{
	sample_hold_set_type(envf->sh, type);
}

bool
envf_set_adsr_active(env_filter* envf, bool act)
{

	return sample_hold_set_active(envf->sh, act);

}

void
envf_set_adsr_atk(env_filter* envf, float atk_)
{
	float x = atk_;
	if(x > 1000.0) 
		x = 1000.0;
	else if (x < 0.0)
		x = 0.0;
	adsr_set_attack(envf->sh->ad, x);
}

void
envf_set_adsr_dcy(env_filter* envf, float dcy_)
{
	float x = dcy_;
	if(x > 1000.0) 
		x = 1000.0;
	else if (x < 0.0)
		x = 0.0;
	adsr_set_decay(envf->sh->ad, x);	
}

void
envf_set_adsr_stn(env_filter* envf, float stn_)
{
	float x = stn_;
	if(x > 1.0) 
		x = 1.0;
	else if (x < 0.0)
		x = 0.0;
	adsr_set_sustain(envf->sh->ad, x);
}

void
envf_set_adsr_rls(env_filter* envf, float rls_)
{
	float x = rls_;
	if(x > 1000.0) 
		x = 1000.0;
	else if (x < 0.0)
		x = 0.0;
	adsr_set_release(envf->sh->ad, x);
}


