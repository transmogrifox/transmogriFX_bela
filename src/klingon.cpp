#include <math.h>
#include <stdlib.h>

#include "klingon.h"

// Allocate the klingon struct and set default values
klingon* make_klingon(klingon* kot, unsigned int oversample, unsigned int bsz, float fs)
{
    kot = (klingon*) malloc(sizeof(klingon));
    kot->procbuf = (float*) malloc(sizeof(float)*bsz*oversample);

    for(int i=0; i<bsz; i++)
    {
        kot->procbuf[i] = 0.0;
    }
    kot->xn1 = 0.0;
    kot->xc1 = 0.0;

    kot->blksz = bsz;
    kot->oversample = oversample;
    kot->fs = fs;
    kot->clipper_fs = ((float) oversample)*fs;
    kot->inverse_oversample_float = 1.0/((float) oversample);

    // Set defaults
    kot->gain = 50.0;
    kot->tone = 0.5; // Not used -- initialized in tonestack
    kot->dry = 1.0;
    kot->level = 0.5;
    kot->bypass = true;

    // Setup EQ stages
    compute_filter_coeffs_1p(&(kot->anti_alias), LPF1P, kot->clipper_fs, kot->fs/4.0);  // down-play any aliasing artefacts
    compute_filter_coeffs_1p(&(kot->pre_emph482), HPF1P, kot->fs, 482.29);
    compute_filter_coeffs_1p(&(kot->pre_emph589), HPF1P, kot->fs, 589.46);
    kotstack_init(&(kot->stack), kot->fs);  // Tonestack

    // First stage gains
    // ( g482*pre_emph482(x) + g589*pre_emph589(x) )*gain + x
    kot->g482 = 1.0/33.0;  //1/33k resistor ratio
    kot->g589 = 1.0/27.0;  //1/27k resistor ratio
                          // 1rst stage gain will be 10 to 110

    // Next compute filter cut-offs and inter-stage gains as dependent upon
    // gain pot rotational postion
    //
    
    // Extract pot resistance from gain (dB) setting
    float pot_fb = (kot->gain - 10.0);  // Portion in op amp feedback
    float pot_ff = 100.0 - pot_fb + 10.0; // Portion in series with following gain stage input
    
    float c_fb = 100e-12;  // Sets low-pass roll-off
    float c_ff = 100e-9;   // Sets high-pass feeding into second gain stage
    
    float fc_fb = 1.0/(2.0*M_PI*10000.0*c_fb);  // Low pass cut-off
    float fc_ff = 1.0/(2.0*M_PI*1000.0*pot_ff*c_ff);     // High-pass cut-off
    
    compute_filter_coeffs_1p(&(kot->pre_emph159), HPF1P, kot->fs, fc_ff);
    compute_filter_coeffs_1p(&(kot->post_emph), LPF1P, kot->clipper_fs, fc_fb);
    
    //Second stage gains
    kot->g159 = 220.0/pot_ff;  // 220k/(10k + (1-x)*100k), ratio of 100k gain pot
    kot->gclip = 6.8/pot_ff;   // 6.8k/(10k + (1-x)*100k), ratio of 100k gain pot

    return kot;
}

void klingon_cleanup(klingon* kot)
{
    free(kot->procbuf);
    free(kot);
}

inline float sqr(float x)
{
    return x*x;
}

//
// Clipping functions
//

// Quadratic clipping function
// Linear between nthrs and thrs, uses x - a*x^2 type of function above threshold

void clipper_tick(klingon* kot, int N, float* x, float* clean)  // Add in gain processing and dry mix
{
	// Hard-coded constants
	static const float kthrs = 0.8;
	static const float knthrs = -0.72;
	static const float kf=1.25;

	float xn = 0.0;
	float cn = 0.0;
	float dx = 0.0;
	float dc = 0.0;
	float delta = 0.0;
	float deltac = 0.0;

    for(int i=0; i<N; i++)
    {
    	// Compute deltas for linear interpolation (upsampling)
    	dx = (x[i] - kot->xn1)*kot->inverse_oversample_float;
    	dc = (clean[i] - kot->xc1)*kot->inverse_oversample_float;
    	
    	// Run clipping function at higher sample rate
    	for(int n = 0; n < kot->oversample; n++)
    	{
    		xn = kot->xn1 + delta; // Linear interpolation up-sampling
    		cn = kot->xc1 + deltac; // Upsample clean signal for mix
    		delta += dx;
    		deltac += dc;
    		
	        //Hard limiting, roughly 2 Si diode drops 1 way, and Si + Schottky other way, 
	        if(xn >= 1.2) xn = 1.2;
	        else if(xn <= -1.12) xn = -1.12;
	        

	        //Soft clipping
	        if(xn > kthrs)
	            xn -= kf*sqr(xn - kthrs);
	        if(xn < knthrs)
	            xn += kf*sqr(xn - knthrs);
	        
	        // Second gain stage op amp clipping if driven hard
	        xn += kot->dry*cn;
	        if(xn > 3.2)
	        	xn = 3.2;
        	else if (xn < -3.2)
        		xn = -3.2;
        		
	        // Pre-filter for down-sampling
	        // Run de-emphasis and anti-aliasing filters
	        xn = tick_filter_1p(&(kot->post_emph), xn);  // Strictly would appear before first stage clipping,
	                                                     // but filter used here for anti-aliasing
	        xn = tick_filter_1p(&(kot->anti_alias), xn);
    	}

    	// Reset linear interpolator state variables
    	kot->xn1 = x[i];
    	kot->xc1 = clean[i];
	    delta = 0.0;
	    deltac = 0.0;

	    // Zero-order hold downsampling assumes de-emphasis filter and anti-aliasing
	    // filters sufficiently rejected harmonics > 1/2 base sample rate
        x[i] = xn;
    }
}

// Typical real-time user-configurable parameters
void kot_set_drive(klingon* kot, float drive_db)   // 0 dB to 45 dB
{
    float drv = drive_db;

    if (drv < 0.0)
        drv = 0.0;
    else if(drv > 45.0)
        drv = 45.0;

    // Convert gain given in dB to absolute value
    drv = powf(10.0,(drv + 0.341)/20.0);

    // Work backward through gain stages to get pot value adjustment
    float gl = 33.0*27.0/(33.0 + 27.0);
    float gb = 22.0;

    // pot setting required to get requested gain
    kot->gain =  (drv/gb - 1.0)*gl;
    
    //
    // Next compute filter cut-offs and inter-stage gains as dependent upon
    // gain pot rotational postion
    //
    
    // Extract pot resistance from gain (dB) setting
    float pot_fb = (kot->gain - 10.0);  // Portion in op amp feedback
    float pot_ff = 100.0 - pot_fb + 10.0; // Portion in series with following gain stage input
    
    float c_ff = 100e-9;   // Sets high-pass feeding into second gain stage
    
    float fc_ff = 1.0/(2.0*M_PI*1000.0*pot_ff*c_ff);     // High-pass cut-off
    
    compute_filter_coeffs_1p(&(kot->pre_emph159), HPF1P, kot->fs, fc_ff);

    //Second stage gains
    kot->g159 = 220.0/pot_ff;  // 220k/(10k + (1-x)*100k), ratio of 100k gain pot
    kot->gclip = 6.8/pot_ff;   // 6.8k/(10k + (1-x)*100k), ratio of 100k gain pot
    
}

//
// Tone Knob setting
//   High-pass cut
// 
void kot_set_tone(klingon* kot, float hf_level_db) 
{
	// Range limiting and converter from logarithmic function
	// to linear function of pot resistance ratios used for
	// computing tonestack IIR coefficients
    float tone = hf_level_db;
    if (tone < -60.0)
        tone = -60.0;
    else if (tone > 0.0)
        tone = 0.0;

    kot->tone = powf(10.0, tone/20.0);  // not used elsewhere, might delete
    
    // Tone control moved to kot_tonestack.h/cpp 
    kotstack_set_tone(&(kot->stack), kot->tone);
}

void kot_set_boost(klingon* kot, float boost) // second high pass cut
{
    // kotstack_set_boost handles out-of-range error checking
    kotstack_set_boost(&(kot->stack), boost);
}

void kot_set_mix(klingon* kot, float dry)  // Dry/Wet control, 0.0 to 1.0
{
	float mix = dry;
	if(dry > 1.0)
		mix = 1.0;
	else if (dry < 0.0)
		mix = 0.0;
	else
		mix = dry;
	kot->dry = mix;
}

void kot_set_level(klingon* kot, float outlevel_db) // -40 dB to +0 dB
{
    float vol = outlevel_db;

    if (vol < -40.0)
        vol = -40.0;
    if (vol > 0.0)
        vol = 0.0;
    kot->level = powf(10.0, vol/20.0);
}

bool kot_set_bypass(klingon* kot, bool bypass)
{
	if(!bypass)
	{
		if(kot->bypass)
			kot->bypass = false;
		else
        {
            kot->bypass = true;
        }
	}
	else
	{
		kot->bypass = true;
	}

	return kot->bypass;
}

// Run the klingon effect
void klingon_tick(klingon* kot, float* x)
{
    unsigned int n = kot->blksz;

    if(kot->bypass)
        return;

    // Run pre-emphasis filters
    for(int i = 0; i<n; i++)
    {
        kot->procbuf[i] = tick_filter_1p(&(kot->pre_emph589), kot->g589*x[i]);
        kot->procbuf[i] += tick_filter_1p(&(kot->pre_emph482), kot->g482*x[i]);
        kot->procbuf[i] *= kot->gain;
        kot->procbuf[i] += x[i];
        /* Op amp hard clipping would be implemented here if thought to make an audible difference */
        x[i] = tick_filter_1p(&(kot->pre_emph159), kot->procbuf[i]);
        kot->procbuf[i] = x[i]*kot->g159;
        x[i] *= kot->gclip;  // This stores the clean portion of the mix
    }

    // Run the clipper
    clipper_tick(kot, n, kot->procbuf, x);  // Quadratic function: x - a*x^2

    // Output level and tone control
    for(int i = 0; i<n; i++)
    {
    	x[i] = kot->level*kotstack_tick(&(kot->stack), kot->procbuf[i]);
    }
}
