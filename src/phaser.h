
#ifndef PHASER_H
#define PHASER_H

#include <lfo.h>

#define PHASER_MAX_STAGES      24

#define PHASER_DEFAULT  0
#define PHASE_90		1

typedef struct phaser_t {
	bool bypass;
	bool reset; //used to tell whether state variables have been reset
	float fs; //sampling frequency
	
	//Number of phaser stages
    size_t n_stages;

    //Wet/Dry mix
    float wet, dry;

    //sum of feeback going into input of phaser
    float fb;

    //whether to add feedback from output of stage N
    bool apply_feedback[PHASER_MAX_STAGES];
    bool stagger[PHASER_MAX_STAGES];

    //amount of distortion to apply
    float distort;    //pre-gain
    float idistort;   //recovery (correction) gain, nominally 1/distort

    //First order high-pass filter coefficients
    //1-pole high pass filter coefficients
    // H(z) = g * (1 - z^-1)/(1 - a1*z^-1)
    // Direct Form 1:
    //      y[n] = ghpf * ( x[n] - x[n-1] ) - a1p*y[n-1]
    float a1p[PHASER_MAX_STAGES];
    float a1p_min[PHASER_MAX_STAGES];
    float a1p_max[PHASER_MAX_STAGES];
    float a1p_dif[PHASER_MAX_STAGES];
    float ghpf[PHASER_MAX_STAGES];

    float w_min[PHASER_MAX_STAGES];
    float w_diff[PHASER_MAX_STAGES];
    float w_max[PHASER_MAX_STAGES];

    //First order high-pass filter state variables
    float yh1[PHASER_MAX_STAGES];
    float xh1[PHASER_MAX_STAGES];

    // 1-pole APF state variable, ph1 = yn_hpf - 2.0*xn
    float ph1[PHASER_MAX_STAGES];
    float gfb[PHASER_MAX_STAGES];

    //modulation
    lfoparams* mod;

} phaser_coeffs;

//Initialize the struct and allocate memory
phaser_coeffs* make_phaser(phaser_coeffs*, float);

//(float x, float gp, iwah_coeffs* cf)
// input sample, pot gain (0...1), iwah struct
float phaser_tick(float, phaser_coeffs*);
void phaser_tick_n( phaser_coeffs*, int, float*);

//Select preset circuit voicings
void phaser_circuit_preset(int , phaser_coeffs* );

//Settings
bool phaser_toggle_bypass(phaser_coeffs* cf);
void phaser_set_mix(phaser_coeffs* cf, float wet);
void phaser_set_nstages(phaser_coeffs* cf, int nstages);
void phaser_set_lfo_type(phaser_coeffs* cf, int n);
void phaser_set_lfo_rate(phaser_coeffs* cf, float rate);
void phaser_set_lfo_depth(phaser_coeffs* cf, float depth, int stage);
void phaser_set_lfo_width(phaser_coeffs* cf, float width, int stage);
void phaser_set_feedback(phaser_coeffs* cf, float fb, int stage);
void phaser_set_distortion(phaser_coeffs* cf, float d);

#endif //PHASER_H
