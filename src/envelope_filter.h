//
// Envelope filter based on state variable filter
// -----------------------------------------------------------------------------
// This is free and unencumbered software released into the public domain.
//
// For more information, please refer to <http://unlicense.org/>
// terms re-iterated in UNLICENSE.txt
// -----------------------------------------------------------------------------
//

#ifndef ENVELOPE_F_H
#define ENVELOPE_F_H
#include "svf.h"
#include "sample_hold_modulator.h"

typedef struct env_filter_t
{
	//Operating conditions
    float fs, ts;   //Sampling Frequency
    float nyquist;
    int N;

    //Frequency range
    float fl, fh;
    float depth;
    float width;
    float *frq;
    
    //Sample/hold/sequencer params
    sh_mod* sh;
    float sh_width;
    float* sh_buff;
    float shmix, ishmix;
    
    //Other params
    bool bypass;
    float mix_wet, mix_dry;
    float thrs, knee, knee_db;
    float* y;

	//Attack and release filter and state variables
    float atk_a, atk_b;
    float rls_a, rls_b;
    float yn;  //attack release filter state variable
    float sns;

    sv_filter* svf;

} env_filter;

env_filter*
envf_make_filter(env_filter* envf, float fs, int N);

//Processing functions
void
envf_tick_n(env_filter* envf, float* x, float* e);

//settings
bool
envf_toggle_bypass(env_filter* envf);

void
envf_set_bypass(env_filter* envf, bool bp);

void
envf_set_q(env_filter* envf, float Q);

void
envf_set_drive(env_filter* envf, float drive_);

void 
envf_set_mix(env_filter* envf, float mix_);

void
envf_set_mix_lpf(env_filter* envf, float mix_);

void
envf_set_mix_bpf(env_filter* envf, float mix_);

void
envf_set_mix_hpf(env_filter* envf, float mix_);

void
envf_set_normalize(env_filter* envf, bool n);

void
envf_set_atk(env_filter* envf, float t);

void
envf_set_rls(env_filter* envf, float t);

void
envf_set_outclip(env_filter* envf, bool clip_output);

void
envf_set_lfo_rate(env_filter* envf, float r_);

void
envf_set_lfo_width(env_filter* envf, float w_);

void
envf_set_width(env_filter* envf, float w_);

void
envf_set_depth(env_filter* envf, float d_);

void
envf_set_sensitivity(env_filter* envf, float sns_);

void
envf_set_gate(env_filter* envf, float thrs_);

void
envf_set_gate_knee(env_filter* envf, float knee_);

//SAMPLE/HOLD AND ADSR
  //If act is true, active state is toggled
  //If act is false, adsr is disabled
  //To force enable then call with act == false followed by act == true,
  //else call with act == true, test returned state and repeat if return value is false.
bool
envf_set_adsr_active(env_filter* envf, bool act);

void
envf_set_mix_sh_modulator(env_filter* envf, float mix_);

void
envf_set_sample_hold_type(env_filter* envf, int type);

void
envf_set_adsr_atk(env_filter* envf, float atk_);

void
envf_set_adsr_dcy(env_filter* envf, float dcy_);

void
envf_set_adsr_stn(env_filter* envf, float stn_);

void
envf_set_adsr_rls(env_filter* envf, float rls_);

#endif //ENVELOPE_F_H


