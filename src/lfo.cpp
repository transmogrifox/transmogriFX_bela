#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lfo.h"

lfoparams* init_lfo(lfoparams* lp, float fosc, float fs, float phase)
{
    lp = (lfoparams*) malloc(sizeof(lfoparams));
    
    float ts = 1.0/fs;
    float frq = 2.0*fosc;
    float t = 4.0*frq*frq*ts*ts;

    float p = phase/180.0;  //Phase can be as large as desired to delay LFO startup
    if(p<0.0) p *= -1.0;  //don't let it be negative
    p /= frq;
    p *= fs;
    int phdly = (int) p;
    lp->startup_delay = phdly;

    //Integrated triangle wave LFO (quasi-sinusoidal, linear pitch shift)
    lp->k_ = lp->k = 2.0*ts*frq;
    lp->nk_ = lp->nk = -2.0*ts*frq;
    lp->psign_ = lp->psign = t;
    lp->nsign_ = lp->nsign = -t;
    lp->sign_ = lp->sign = t;
    lp->lfo = 0.0;
    lp->x = 0.0;
    
    //Triangle wave LFO variables
    lp->ktri = frq/fs;
    lp->trisign = 1.0;
    p = frq*phase/(360.0*fosc);
    if(p >= 1.0) 
    {
        p -= 1.0;
        lp->trisign = -1.0;
    }
    if(p<0.0)
    {
        p = 0.0;
        lp->trisign = 1.0;
    }
    lp->trilfo = p;
    
    //Sine wave LFO variables
    lp->ksin =  M_PI*frq/fs;
    lp->sin_part = sin(2.0*M_PI*phase/360.0);
    lp->cos_part = cos(2.0*M_PI*phase/360.0);
    
    //Relaxation oscillator parameters
    float ie = 1.0/(1.0-1.0/M_E);
    float k = expf(-2.0*fosc/fs);

    lp->rlx_k = k;
    lp->rlx_ik = 1.0 - k;

    lp->rlx_sign = ie;
    lp->rlx_max = ie;
    lp->rlx_min = 1.0 - ie;

    lp->rlx_lfo = 0.0;

    //Exponential oscillator parameters
    k = expf(-2.0*1.3133*fosc/fs);
    lp->exp_ik = k;
    lp->exp_k = 1.0/k;
    lp->exp_x = k;
    lp->exp_min = 1.0/M_E;
    lp->exp_max = 1.0 + 1.0/M_E;
    lp->exp_sv = lp->exp_min;
    
    //Globals
    lp->current_rate = fosc;
    lp->lfo_type = 0; //integrated triangle
    
    return lp; 
}

void update_lfo(lfoparams* lp, float fosc, float fs)
{

    float ts = 1.0/fs;
    float frq = 2.0*fosc;
    float t = 4.0*frq*frq*ts*ts;
    
    //record the new setting 
    lp->current_rate = fosc;
    
    //integrated triangle LFO 
    lp->k_ = 2.0*ts*frq;
    lp->nk_ = -2.0*ts*frq;
    lp->psign_ = t;
    lp->nsign_ = -t;
    lp->sign_ = t;
    
    //Triangle LFO
    lp->ktri = frq/fs;
    
    //Sine LFO
    lp->ksin =  M_PI*frq/fs;

    //Relaxation oscillator parameters
    float k = expf(-2.0*fosc/fs);
    lp->rlx_k = k;
    lp->rlx_ik = 1.0 - k;
    
    //Exponential oscillator parameters
    k = expf(-2.0*1.3133*fosc/fs);
    lp->exp_ik = k;
    lp->exp_k = 1.0/k;
    
    if(lp->exp_x >= 1.0)
    	lp->exp_x = lp->exp_k;
	else
		lp->exp_x = lp->exp_ik;
		
    lp->exp_min = 1.0/M_E;
    lp->exp_max = 1.0 + 1.0/M_E;
    if(lp->exp_sv < lp->exp_min) lp->exp_sv = lp->exp_min;
    if(lp->exp_sv > lp->exp_max) lp->exp_sv = lp->exp_max;

}

//
// Integrated Triangle LFO shape 
//   The LFO shape appears sinusoidal but the derivative is 
//   a triangle wave.
//   This produces a linear (triangle) pitch shift when applied to 
//   a modulated delay line.
//

float run_integrated_triangle_lfo(lfoparams* lp)
{
    //Wait for startup delay to begin at requested phase
    if(lp->startup_delay > 0) {
        lp->startup_delay -= 1;
        lp->lfo = 0.0;
        return 0.0;
    }
    
    lp->x += lp->sign;
    if(lp->x >= lp->k){
        lp->sign = lp->nsign_;
        
        //Reset oscillator if rate change
        lp->x = lp->k_;
        lp->k = lp->k_;
        lp->nk = lp->nk_;
        
    } 
    else if (lp->x <= lp->nk) {
        lp->sign = lp->psign_;

        //Reset oscillator if rate change
        lp->x = lp->nk_;
        lp->k = lp->k_;
        lp->nk = lp->nk_;
    }

    
    lp->lfo += lp->x;
    if(lp->lfo > 1.0) {
        //fprintf(stderr,"%f\n",sum);
        lp->lfo = 1.0;
    }
    if(lp->lfo < 0.0) {
        //fprintf(stderr,"%f\n",sum);
        lp->lfo = 0.0;
    }
    return lp->lfo;
}

// Triangle wave oscillator
float run_triangle_lfo(lfoparams* lp)
{
        
    lp->trilfo += lp->ktri*lp->trisign;
    if(lp->trilfo >= 1.0) {
        lp->trisign = -1.0;
    }
    if(lp->trilfo <= 0.0) {
        lp->trisign = 1.0;
    }
    return lp->trilfo;
}

//Sine oscillator
float run_sine_lfo(lfoparams* lp)
{
    lp->sin_part += lp->cos_part*lp->ksin;
    lp->cos_part -= lp->sin_part*lp->ksin;
    return 0.5*(1.0 +lp->cos_part);
}


//
// RC Relaxation oscillator
// Simple 1rst-order lowpass filter driven by rlx_sign variable
// rlx_sign is toggled between 1.0 + 1/e and -1/e and
// toggles when LPF output reaches thresholds 0.0 and 1.0
// This relationship makes frequency computation simple
// as the oscillation period is simply 2*Tau (2RC)
//
// The waveshape is the typical RC filter (1-e^-t/Tau) charge
// shape followed by the RC filter exponential discharge shape.
// It appears roughly triangular.
// 
// This could be abused by adjusting rlx_min and rlx_max values
// to asymmetrical values, or closer to 1.0 to flatten the top more.
// 
float run_rlx_lfo(lfoparams* lp)
{
    //Simple 1rst-order lowpass filter
    lp->rlx_lfo = lp->rlx_sign * lp->rlx_ik + lp->rlx_k * lp->rlx_lfo;
    
    //Schmitt trigger logic
    if(lp->rlx_lfo >= 1.0) {
        lp->rlx_sign = lp->rlx_min;
    } 
    else if(lp->rlx_lfo <= 0.0) 
    {
        lp->rlx_sign = lp->rlx_max;
    }
    
    //Output
    return lp->rlx_lfo;
}

float run_exp_lfo(lfoparams* lp)
{
    //Exponential oscillator parameters
    lp->exp_sv *= lp->exp_x;
    if(lp->exp_sv >= lp->exp_max) {
        lp->exp_x = lp->exp_ik;
    } else if (lp->exp_sv <= lp->exp_min) {
        lp->exp_x = lp->exp_k;
    }
    
    return lp->exp_sv - lp->exp_min;
}

float run_lfo(lfoparams* lp)
{
    float lfo_out = 0.0;
    switch(lp->lfo_type)
    {
        case INT_TRI: //integrated triangle
            lfo_out = run_integrated_triangle_lfo(lp);
            break;
            
        case TRI: //triangle 
            lfo_out = run_triangle_lfo(lp);
            break;
        
        case SINE: //sine
            lfo_out = run_sine_lfo(lp);
            break;
        
        case SQUARE: //click-less square (compressed sine)
            lfo_out = run_sine_lfo(lp) - 0.5;
            
            //Amplify and soft-clip the sine wave
            if(lfo_out > 0.0) {
                lfo_out *= 1.0/(1.0+30.0*lfo_out);
            } else {
                lfo_out *= 1.0/(1.0-30.0*lfo_out);
            }
            lfo_out *= 16.0;
            lfo_out += 0.5;
            
            break;
        
        case EXP: //exponential
            lfo_out = run_exp_lfo(lp);
            break;
        
        case RELAX: //RC relaxation oscillator
            lfo_out = run_rlx_lfo(lp);
            break;
            
        case HYPER: //smooth bottom, triangular top
        	lfo_out = run_integrated_triangle_lfo(lp);
        	lfo_out = 1.0 - fabs(lfo_out - 0.5);
        	break;
    	
    	case HYPER_SINE:  //Sine bottom, triangular top
        	lfo_out = run_sine_lfo(lp);
        	lfo_out = 1.0 - fabs(lfo_out - 0.5);
        	break;    	
            
        default:
            lfo_out = run_integrated_triangle_lfo(lp);
            break;
    }
    
    return lfo_out;
}

void get_lfo_name(unsigned int type, char* outstring)
{
	int i;
	for(i=0; i<30; i++)
		outstring[i] = '\0';
		
   switch(type)
    {
        case INT_TRI: //integrated triangle
            sprintf(outstring, "INTEGRATED TRIANGLE");
            break;
            
        case TRI: //triangle 
            sprintf(outstring, "TRIANGLE");
            break;
        
        case SINE: //sine
            sprintf(outstring, "SINE");
            break;
        
        case SQUARE: //click-less square (compressed sine)
            sprintf(outstring, "SQUARE");
            break;
        
        case EXP: //exponential
            sprintf(outstring, "EXPONENTIAL");
            break;
        
        case RELAX: //RC relaxation oscillator
            sprintf(outstring, "RC RELAXATION");
            break;
        case HYPER: //RC relaxation oscillator
            sprintf(outstring, "HYPER");
            break;
        case HYPER_SINE: //RC relaxation oscillator
            sprintf(outstring, "HYPER_SINE");
            break;
        default:
            sprintf(outstring, "DEFAULT: INTEGRATED TRIANGLE");
            break;
    }
    

}

void set_lfo_type(lfoparams* lp, unsigned int type) 
{
    lp->lfo_type = type;
}


