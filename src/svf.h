//
// Double-samplerate state variable filter using
// linear interpolated over-sampling.
// -----------------------------------------------------------------------------
// This is free and unencumbered software released into the public domain.
//
// For more information, please refer to <http://unlicense.org/>
// terms re-iterated in UNLICENSE.txt
// -----------------------------------------------------------------------------
//
//  State Variable Filter Notes:
//  To emulate a classic guitar effect the center frequency should be modulated
//  linearly. Fortunately over the majority of the range where a guitar effect
//  is used, the digital state variable has a linear relationship between
//  the "f" coefficient and resonant frequency, "fc".
//
//  Therefore the most efficient way to modulate this filter is to pre-compute
//  "f" for the min frequency and "f" for the max frequency and scale the
//  LFO, ADSR or envelope detector output into that range.
//
//  Reference Classic Guitar Effect:
//  A Mutron III envelope filter exhibits a nearly linear transfer from
//  CV to resonant frequency, which then rounds off slightly at the higher
//  and lower ends of the range where parallel resistor dominates (on low end)
//  and vactrol begins to reach its limit on the high end.
//
//  Mutron III frequency range:
//  The Mutron III bottoms out at about 500 Hz for "high" range setting and
//  about 220 Hz for the "low" range setting, which is reasonably predictable.
//  The upper limits of the sweep are highly variable, but 1500 to 2500 Hz is
//  recommended for guitar use.
//
//  Explantation
//  The vactrol LED current to resistance follows a 1/x type of curve over
//  most of its range.  A Mutron III circuit controls LED current linearly
//  with envelope detector output voltage.
//  The analog state-variable-filter resonant frequency
//  is expressed by fc = 1/(2*pi*R*C).  If R ~ 1/i, then you get
//  1/(2*pi*(k/i)*C) ==> i/(2*pi*C*k), where k is a scale factor setting
//  proportional relationship between R and LED current, i.
//
//  This i-to-R transfer was traced from a Vactrol VTL-5C3 datasheet graph
//

#ifndef SVF_H
#define SVF_H


typedef struct sv_filter_t
{
    float fs;   //Sampling Frequency
    float fc;   //Filter resonance frequency

    //User options
      //filter output mixing coefficients:
    float lmix, bmix, hmix;
      //gain and clipping options
    bool normalize;
    bool outclip;

    //variables depending on fc, fs
    float f, fcnst;
    float q;
    float g, g5, gout;
    float drive, idrive;

    //State Variables
    float lpf, hpf, bpf;
    float x1;
    float f1;
} sv_filter;

sv_filter*
svf_make_filter(sv_filter* s, float fs);

//Processing functions
  //Run the filter at a fixed Q and center frequency.
  //Useful for filters that aren't continuously changed
void
svf_tick_n(sv_filter* s, float* x, int N);

  //Functions to continuously modulate filter with parameter f.
    //No distortion, no output limiting
void
svf_tick_fmod_n(sv_filter* s, float* x, float *f, int N);

  //Set drive level with "svf_set_drive()" to adjust amount of distortion
void
svf_tick_fmod_soft_clip_n(sv_filter* s, float* x, float *f, int N);

//settings
float
svf_compute_f (sv_filter* s, float fc);

void
svf_set_q(sv_filter* s, float Q);

void
svf_set_drive(sv_filter* s, float drive_);

void
svf_set_mix_lpf(sv_filter* s, float mix_);

void
svf_set_mix_bpf(sv_filter* s, float mix_);

void
svf_set_mix_hpf(sv_filter* s, float mix_);

void
svf_set_normalize(sv_filter* s, bool n);

void
svf_set_outclip(sv_filter* s, bool clip_output);

#endif //SVF_H
