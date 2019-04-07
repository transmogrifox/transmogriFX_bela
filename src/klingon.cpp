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
    kot->gain = 30.0;
    kot->tone = 0.5;
    kot->level = 0.5;
    kot->bypass = true;

    // Setup EQ stages
    compute_filter_coeffs_1p(&(kot->anti_alias), LPF1P, kot->clipper_fs, 4410.0);  // 1/10th fs assuming 44.1k rate -- increasing filter order will help
                                                                              // down-play any aliasing artefacts, but increases CPU usage
    compute_filter_coeffs_1p(&(kot->pre_emph482), HPF1P, kot->fs, 482.29);
    compute_filter_coeffs_1p(&(kot->pre_emph589), HPF1P, kot->fs, 589.46);
    compute_filter_coeffs_1p(&(kot->pre_emph589), HPF1P, kot->fs, 159.15);

    compute_filter_coeffs_1p(&(kot->post_emph), LPF1P, kot->clipper_fs, 14500.0);
    compute_filter_coeffs_1p(&(kot->tone_lp), LPF1P, kot->fs, 636.0);

    // First stage gains
    // (g482*pre_emph482(x) + g589*pre_emph589(x))*gain + x
    kot->g482 = 1.0/33.0;  //1/33k resistor ratio
    kot->g589 = 1.0/27.0;  //1/27k resistor ratio
                          // 1rst stage gain will be 10 to 110
    //Second stage gain
    kot->g159 = 220.0/10.0;  // 220k/10k
    kot->gclip = 6.8/10.0;   // 6.8k/10k

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
float kthrs = 0.8;
float knthrs = -0.72;
float kf=1.25;
void clipper_tick(klingon* kot, int N, float* x, float* clean)  // Add in gain processing and dry mix
{
	float xn = 0.0;
	float dx = 0.0;
	float dc = 0.0;
	float delta = 0.0;
	float deltac = 0.0;

    for(int i=0; i<N; i++)
    {
    	// Compute deltas for linear interpolation (upsampling)
    	dx = (x[i] - kot->xn1)*kot->inverse_oversample_float;
    	dc = (clean[i] - kot->xc1)*kot->inverse_oversample_float;
    	kot->xn1 = x[i];
    	kot->xc1 = clean[i];

    	// Run clipping function at higher sample rate
    	for(int n = 0; n < kot->oversample; n++)
    	{
    		xn = x[i] + delta; // Linear interpolation up-sampling
    		clean[i] = clean[i] + deltac; // Upsample clean signal for mix
    		delta += dx;
    		deltac += dc;
	        //Hard limiting
	        if(xn >= 1.2) xn = 1.2;
	        if(xn <= -1.12) xn = -1.12;

	        //Soft clipping
	        if(xn > kthrs){
	            xn -= kf*sqr(xn - kthrs);
	        }
	        if(xn < knthrs){
	            xn += kf*sqr(xn - knthrs);
	        }

	        // Pre-filter for down-sampling
	        // Run de-emphasis and anti-aliasing filters
	        xn = tick_filter_1p(&(kot->post_emph), (clean[i] + 2.0*xn));
	        xn = tick_filter_1p(&(kot->anti_alias), xn);
    	}

    	// Reset linear interpolator state variables
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
    drv = powf(10.0,drv/20.0);

    // Work backward through gain stages to get pot value adjustment
    float gl = 33.0*27.0/(33.0 + 27.0);
    float gb = 22.0;

    // pot setting required to get requested gain
    kot->gain =  (drv/gb - 1.0)*gl;
}

void kot_set_tone(klingon* kot, float hf_level_db) // high pass boost/cut, +/- 12dB
{
    float tone = hf_level_db;

    if (tone < -60.0)
        tone = -60.0;
    else if (tone > 0.0)
        tone = 0.0;

    kot->tone = powf(10.0, tone/20.0);
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
        kot->procbuf[i] += tick_filter_1p(&(kot->pre_emph159), kot->g159*x[i]);
        x[i] *= kot->gclip;  // This stores the clean portion of the mix
    }

    // Run the clipper
    clipper_tick(kot, n, kot->procbuf, x);  // Quadratic function: x - a*x^2

    // Output level and tone control
    for(int i = 0; i<n; i++)
    {
        x[i] = kot->tone*kot->procbuf[i] + (1.0 - kot->tone)*tick_filter_1p(&(kot->tone_lp), kot->procbuf[i]);
        x[i] *= kot->level;
    }
}
