//
//   Sample/hold modulator
//
// -----------------------------------------------------------------------------
// This is free and unencumbered software released into the public domain.
//
// For more information, please refer to <http://unlicense.org/>
// terms re-iterated in UNLICENSE.txt
// -----------------------------------------------------------------------------
//

#ifndef SH_MOD_H
#define  SH_MOD_H

#include "adsr.h"

#define MAX_SEQ_STEPS       12

#define SH_RAND             0  //Sample random sequence
#define SH_RAMP             1  //Sample a ramp oscillator
#define SH_SEQ              2  //Run as sequencer
#define SH_MAX_TYPES		3

typedef struct sh_mod_t
{
    //System properties
    float fs, ifs;          //Sampling frequency and its inverse
    int NS;                 //Number of samples in loop
    float max_types;

    // Parameters
    float* sequence;         //Run as sequencer
    int n_seq_steps;          //Number of steps in sequence
    float dl;               //change between last sample to previous sample
    float dt_lfo, dt_hfo;   //Ramp oscillator rates
    float steepness;        //How abbruptly it steps from last sample to current
    int  mode;
    bool en_adsr;

    //State Variables
    int seq_step;           //Current step in the sequence
    float ramp_lfo;         //LFO ramp state variable:  sets step rate
    float ramp_hfo;         //sampled signal
    float last_hfo_sample;  //hold state variable
    float last_wave;        //ending point of last step

    //Dependencies
    adsr* ad;
    
    //Outputs and buffered states for block processing
    float *adsr_env;        //output from ADSR
    float *lfo;             //buffer to store NS shaped LFO outputs
    float *hfo;             //buffer to store NS sampled signal outputs
    float *swave;           //Final synthesized stepped LFO shape
    bool *sclk;             //sample clock pulse at LFO reset

} sh_mod;


sh_mod*
make_sample_hold(sh_mod* sh, float fs, int N);

//Output is a buffer assumed to be length N,
//containing the next N samples of the SH
//modulator waveform
void
run_sample_hold(sh_mod* sh, float* output);

//Settings
void
sample_hold_set_rate(sh_mod* sh, float rate);

bool
sample_hold_set_active(sh_mod* sh, bool act);

void
sample_hold_set_type(sh_mod* sh, int type);

void
sample_hold_set_max_sequences(sh_mod* sh, int max_seq);

void
sample_hold_set_sequence_level(sh_mod* sh, int seq, float seq_val);


#endif //SH_MOD_H
