//Band-limited interpolated delay line.  Uses 8th order butterworth IIR filter
//emulating combination of anti-aliasing & reconstruction filters from an 
//analog BBD typical circuit.

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "flange.h"

void tflanger_resize_delay ( tflanger *cthis, float maxTime )
{
    //printf("Updating delay line size.\n");
    size_t nSize = (size_t) lrint(maxTime*cthis->fS + 1.0);
    float *temp = (float*) malloc(nSize*sizeof(float));
    if( temp == NULL ) {
        fprintf(stderr, "Cannot allocate any more memory.  Requested increase of delay time declined");
        return;
    }

    int i;
    for(i=0;i<nSize;i++)
    {
        temp[i] = 0.0;
    }
    for(i=0;i<cthis->maxDly;i++)
    {
        temp[i] = cthis->dlyLine[i];
    }
    
    cthis->maxDly = nSize-1;
    cthis->maxT = maxTime;
    
    float *dead = cthis->dlyLine;
    cthis->dlyLine = temp;
    free(dead);
}

tflanger * 
tflanger_init(tflanger *cthis, float maxTime, float fSampleRate)
{

    cthis = (tflanger*) malloc(sizeof(tflanger));
    cthis->maxT = maxTime;
    cthis->fS = fSampleRate;
    size_t nSize = (size_t) lrint(cthis->maxT*fSampleRate + 1.0);
    cthis->dlyLine = (float*) malloc(nSize*sizeof(float)); 
    int i;
    for(i=0;i<nSize;i++)
    {
        cthis->dlyLine[i] = 0.0;
    }

    cthis->maxDly = nSize-1;
    //printf("maxT = %f\tmaxDly = %d\n", maxT, maxDly);

    //Delay line pointer
    cthis->dlyWrite = 0;

    //LFO
    cthis->lfopar = init_lfo(cthis->lfopar, 1.0, cthis->fS, 0.0);
    cthis->maxLfoRate = 100.0;
    
    //Envelope detector
    cthis->envelope = (fparams*) malloc(sizeof(fparams));
    float tc10 = 0.015;  //About 10 Hz cutoff
    cthis->envelope->alpha = 1.0f/(tc10*cthis->fS +1.0f);
    cthis->envelope->ialpha = 1.0f - cthis->envelope->alpha;
    cthis->envelope->x1 = 0.0;
    cthis->envelope->y1 = 0.0;
    
    //Attack/Release settings
    cthis->attrel = (arparams*) malloc(sizeof(arparams));
    float atkt = 0.05;
    float rlst = 0.5;
    cthis->attrel->y1 = 0.0;
    cthis->attrel->atk = 1.0f/(atkt*cthis->fS +1.0f);
    cthis->attrel->iatk = 1.0f - cthis->attrel->atk;
    cthis-> attrel->rls = 1.0f/(rlst*cthis->fS +1.0f);
    cthis->attrel->irls = 1.0f - cthis->attrel->rls;
    
    //Bypass fader
    cthis->fader = (fparams*) malloc(sizeof(fparams));
    float tcf = 0.25;  //Fader time constant
    cthis->fader->alpha = 1.0f/(tcf*cthis->fS +1.0f);
    cthis->fader->ialpha = 1.0f - cthis->fader->alpha;
    cthis->fader->x1 = 0.0;
    cthis->fader->y1 = 0.0;

    //Set some sane initial values
    float ms = 0.001;
    tflanger_setLfoDepth(cthis, (9.0*ms)); //lfo offset, input in seconds
    tflanger_setLfoWidth(cthis, (0.8*ms)); //lfo deviation, input in seconds will
                                          //express peak-to-trough deviation
    tflanger_setLfoRate(cthis, 3.0);    //Input in cycles per second
    tflanger_setLfoPhase(cthis, 0.0);    //Phase offset, 0 to 2*pi, useful if 
                                        //using 2 of these objects in stereo
    tflanger_setWetDry(cthis, 0.71);     //give ratio wet, 0...1.0. 
                                        //Dry = 1.0 - fracWet
    tflanger_setFeedBack(cthis, 0.0);   //Regen
    
    //Bypass processing
    tflanger_setFinalGain(cthis, 1.0);  // Set to 0.0 for bypass, set to >0.99 for norm
    cthis->trails = 1;					//bypass with trails default

    //Envelope Detector Settings
    tflanger_setEnvelopeRateSkew(cthis, 0.0);
    tflanger_setEnvelopeDepthSkew(cthis, 0.0);
    tflanger_setEnvelopeWidthSkew(cthis, 0.0);
    tflanger_setEnvelopeFbSkew(cthis, 0.0);
    tflanger_setEnvelopeMixSkew(cthis, 0.0);
    tflanger_setEnvelopeSensitivity(cthis, 1.0);

    //Anti-aliasing filter
    float fs = fSampleRate;
    float f0 = 7200.0;
    float* q = NULL;
    q = make_butterworth_coeffs(8, q);
    cthis->f[0] = make_biquad(LPF, cthis->f[0], fs, f0, q[0]);
    cthis->f[1] = make_biquad(LPF, cthis->f[1], fs, f0, q[1]);
    cthis->f[2] = make_biquad(LPF, cthis->f[2], fs, f0, q[2]);   
    cthis->f[3] = make_biquad(LPF, cthis-> f[3], fs, f0, q[3]);
    
    //Feedback tap to be run back through anti-alias filter with input
    cthis->regen = 0.0;
    
    //Final computation
    tflanger_updateParams(cthis);
    
    return cthis;
}


void 
tflanger_destroy(tflanger *cthis)
{
    free(cthis->dlyLine);
    free(cthis);
}

void 
get_wet_dry_mix(float fracWet, float *wet_out, float *dry_out) 
{
	float wet = 1.0;
	float dry = 1.0;
	float sign = 1.0;
	
    if(fracWet >= 1.0) wet = 1.0;
    else if (fracWet<(-1.0)) wet = -1.0; //allow add or subtract
    else wet = fracWet;
    
    if(wet < 0.0) sign = -1.0;
    float x = wet*wet;
    
    // make dry mix function
	float k= x*x;
	k *= k;
	dry = 1.0 - k;
	
	// make wet mix function
	k = 1 - x;
	k *= k;
	k *= k;
    wet = (1.0 - k)*sign;

    //keep dry at 0 phase
    *dry_out = dry;
    *wet_out = wet;

}

float potfunc1(float xn)
{
    float x = 0.0;
    int sign = 0;

    if(xn < 0.0)
    {
        xn = -xn;
        sign = 1;
    }
    if(xn > 1.0) xn = 1.0;

    x = 2.0*xn;

    if(x <= 1.0) xn = 0.5*x*x;
    else xn = 1.0 - (1.0 - xn)*(2.0 - x);

    if(sign == 1) xn = -xn;

    return xn;
}


void 
tflanger_tick(tflanger *cthis, int nframes, float *samples, float* envelope)
{

	if( (cthis->dry0 > 0.999) && (cthis->trails == 0) && (cthis->outGain < 0.1) ) return;
    int i,j;
    float dly = 0.0;
    float dlyFloor = 0.0;
    float fd = 0.5;
    float ifd = 0.5;

    float in = 0.0; //current sample
    float envdet = 0.0;

    long d = 0; //integer delay
    long d1 = 0; //integer delay - 1 

    for(i=0; i < nframes; i++)
    {
    	//Envelope detector
    	envdet = envelope[i];
    	if(envdet > 1.0) envdet = 1.0;
    	else if(envdet < 0.0) envdet = 0.0;
    	
        envdet = tflanger_atkrls(cthis->attrel, envdet);

         // Bypass logic:  LPF set to slow time constant to fade input on and off 
         // fades dry mix to unity in small steps at the same time.
    	in = samples[i];
        tflanger_lpfilter( cthis->fader, cthis->outGain, 0);
        in *= cthis->fader->y1;
        if(cthis->outGain < 0.99){
        	if(cthis->dry0 < 1.0) 
        		cthis->dry0 += 0.0001;  //-80dB steps to unity, about 1/4 second at 44.1 kHz
        }

		// Insert feedback
        in += cthis->regen;

        //anti-alias filter 
        for(j=0;j<4;j++)
        {
            in = run_filter(in, cthis->f[j]);
        }

         //do ring buffer indexing
        if(++(cthis->dlyWrite) >= cthis->maxDly) cthis->dlyWrite = 0; 
        cthis->dlyLine[cthis->dlyWrite] = in;


        //Apply to rate parameter
        if(cthis->rateskew != 0.0) {
            float new_lfo_rate = cthis->rateskew*envdet + cthis->lfoRate;
            if(new_lfo_rate < 0.0) new_lfo_rate = 0.1;
            update_lfo(cthis->lfopar, new_lfo_rate, cthis->fS);
        } else if(cthis->lfoRate != cthis->lfopar->current_rate ) {
            update_lfo(cthis->lfopar, cthis->lfoRate, cthis->fS);
        }

        //Apply to mix
        
        if(cthis->mixskew!=0.0) {
        	if(cthis->wet > 0.0) cthis->wet0 = cthis->mixskew*envdet + cthis->wet;
        	else cthis->wet0 = cthis->wet - cthis->mixskew*envdet;
        	
        	if(cthis->outGain >= 0.99)
            	cthis->dry0 = cthis->dry - cthis->mixskew*envdet;
            
            if(cthis->wet0 > 1.0) cthis->wet0 = 1.0;
            if(cthis->wet0 < -1.0) cthis->wet0 = -1.0;
            if(cthis->dry0 > 1.0) cthis->dry0 = 1.0;
            if(cthis->dry0 < -1.0) cthis->dry0 = -1.0;
        } else {
            cthis->wet0 = cthis->wet;
            if(cthis->outGain >= 0.99)
            	cthis->dry0 = cthis->dry;    
         }

        //Apply to depth
        if(cthis->depthskew!=0.0) {
            cthis->lfoDepth0 = cthis->depthskew*envdet + cthis->lfoDepth;
            if(cthis->lfoDepth0 < 0.0) cthis->lfoDepth0 = 0.0;
            if(cthis->lfoDepth0 >= cthis->maxT) cthis->lfoDepth0 = cthis->maxT;
        } else cthis->lfoDepth0 = cthis->lfoDepth;
        
        //Apply to width
        if(cthis->widthskew!=0.0) {
            cthis->lfoWidth0 = cthis->widthskew*envdet + cthis->lfoWidth;
            if(cthis->lfoWidth0 < 0.0) cthis->lfoWidth0 = 0.0;  
            
            if( (cthis->lfoWidth0 + cthis->lfoDepth0) >= cthis->maxT) {
                cthis->lfoWidth0 = cthis->maxT - cthis->lfoDepth0;
            }
        } else cthis->lfoWidth0 = cthis->lfoWidth;

        //Apply to feedback
        if(cthis->fbskew!=0.0) {
            cthis->fb = cthis->fbskew*envdet + cthis->feedBack;
            if(cthis->fb >= 1.0) cthis->fb = 0.99;   //less extreme limits
            if(cthis->fb <= -1.0) cthis->fb = -0.99;
        } else cthis->fb = cthis->feedBack;
        
        //run LFO
        float lfo = run_lfo(cthis->lfopar);

         //calculate delay variables
         lfo *= cthis->lfoWidth0;
         dly = cthis->fS*(cthis->lfoDepth0 + lfo); 
         dlyFloor = floor(dly);
         fd = (dly - dlyFloor);
         ifd = 1.0 - fd;

         //set up high delay tap
         d = lrint(dlyFloor);
         d1 = d + 1;
         d = cthis->dlyWrite - d;
         d1 = cthis->dlyWrite - d1;

         if(d<0) d += cthis->maxDly;
         if(d1<0) d1 += cthis->maxDly;

         //output signal
         cthis->regen = fd*cthis->dlyLine[d1] + ifd*cthis->dlyLine[d];
         
         if(cthis->trails == 0)
         	samples[i] = cthis->dry0*samples[i] + cthis->wet0*cthis->fader->y1*cthis->regen;
     	else
     		samples[i] = cthis->dry0*samples[i] + cthis->wet0*cthis->regen;

         cthis->regen *= cthis->fb;


    } //for(i...

}


float 
tflanger_lpfilter(fparams *cthis, float data, char mode)
{
    float y0 = 0.0f;
    y0 = cthis->alpha*data + cthis->ialpha*cthis->y1;
    cthis->y1 = y0;
    if (mode == 0) return y0;
    else return (0.5f*(y0+data));  //low-pass shelf where high freqs shelve to 1/2 value of low freqs
}

float 
tflanger_atkrls(arparams *cthis, float data)
{
    float y0 = 0.0f;
    if(data > cthis->y1) y0 = cthis->atk*data + cthis->iatk*cthis->y1;  //Attack
    else y0 = cthis->rls*data + cthis->irls*cthis->y1;  //Release
    cthis->y1 = y0;
    return y0;
}

void
tflanger_updateParams(tflanger *cthis)
{

     if( (cthis->lfoWidth + cthis->lfoDepth) >= cthis->maxT) {
       cthis->lfoWidth =  cthis->maxT - cthis->lfoDepth;
     }
     
     /*
     cthis->lfoWidth0 = cthis->lfoWidth;
     cthis->lfoDepth0 = cthis->lfoDepth;
     cthis->wet0 = cthis->wet;
     cthis->dry0 = cthis->dry;
     cthis->fb = cthis->feedBack;
     */
     
     //printf("\n\nLFO Depth = %lf\nLFO Width = %lf\nLFO Rate = %lf\n%%Wet:  %lf\n%%Feedback: %lf\nOutput Level:  %lf\n", cthis->lfoDepth, cthis->lfoWidth, cthis->lfoRate, cthis->wet, cthis->feedBack, cthis->outGain);
}

void 
tflanger_setLfoDepth(tflanger *cthis, float lfoDepth_)
{
    cthis->lfoDepth = fabs(lfoDepth_);
    //printf("d_ = %lf\tdepth = %lf\n\n", lfoDepth_, lfoDepth);
    if( (2.0*cthis->lfoDepth) >= cthis->maxT ) {
        tflanger_resize_delay ( cthis, 2.0*cthis->lfoDepth );
    }
    
    tflanger_updateParams(cthis);
       // printf("d_ = %lf\tdepth = %lf\n\n", lfoDepth_, lfoDepth);
}

void 
tflanger_setLfoWidth(tflanger *cthis, float lfoWidth_)
{
    cthis->lfoWidth = fabs(lfoWidth_);
    if( (cthis->lfoWidth + cthis->lfoDepth) >= cthis->maxT) {
        tflanger_resize_delay ( cthis, cthis->lfoWidth + cthis->lfoDepth );
    }
   tflanger_updateParams(cthis);
}

void 
tflanger_setLfoRate(tflanger *cthis, float lfoRate_) 
{
    cthis->lfoRate = fabs(lfoRate_);

    if(cthis->lfoRate>=cthis->maxLfoRate) cthis->lfoRate = cthis->maxLfoRate;
    update_lfo(cthis->lfopar, cthis->lfoRate, cthis->fS);

}

void 
tflanger_setLfoPhase(tflanger *cthis, float lfoPhase_)
{
    cthis->lfoPhase = lfoPhase_;
    cthis->lfoPhase = fmod(cthis->lfoPhase, 2.0*M_PI);
    tflanger_updateParams(cthis);
}

void 
tflanger_setWetDry(tflanger *cthis, float fracWet) 
{
	float wet = 1.0;
	float dry = 1.0;

	get_wet_dry_mix(fracWet, &wet, &dry);

    //keep dry at 0 phase
    cthis->dry = dry;
    cthis->wet = wet;
    tflanger_updateParams(cthis);
}

void 
tflanger_setFeedBack(tflanger *cthis, float feedBack_)
{

    cthis->feedBack = feedBack_;//potfunc1(feedBack_);//*(2.0 - fabs(feedBack_));

    if(cthis->feedBack >= 0.99) cthis->feedBack = 0.99;
    else if (cthis->feedBack <= -0.99) cthis->feedBack = -0.99;

}

void
tflanger_setDamping(tflanger *cthis, float fdamp_)
{
	float fdamp = fdamp_;
	
	//Q of the final stage comes out ot about 0.51 
	//so it is a relatively gentle roll-off
	float Q = cthis->f[3]->Q;
	if(fdamp < 40.0) {
		fdamp = 40.0;
	}
	else if (fdamp > 7200.0) {
		fdamp = 7200.0;
	}
	
	// 
	biquad_update_coeffs(LPF, cthis-> f[3], cthis->fS, fdamp, Q);
}


void 
tflanger_setFinalGain(tflanger *cthis, float outGain_)
{
    cthis->outGain = outGain_;
    //limit gain here to something sane
    if(cthis->outGain>=1.42) cthis->outGain = 1.42; //1.42 is about +3dB
    if(cthis->outGain<0.0) cthis->outGain = 0.0;
}

void
tflanger_setTrails(tflanger *cthis, char trails)
{
	cthis->trails = trails;
}

void
tflanger_setEnvelopeSensitivity(tflanger *cthis, float sns)
{
	if(sns < 0.0) 
		cthis->envelope_sensitivity = 0.0;
	else if (sns > 36.0) 
		sns = 36.0;
	else
		cthis->envelope_sensitivity = sns;
}

void
tflanger_setEnvelopeAttack(tflanger *cthis, float atk)
{
    cthis->attrel->atk = 1.0f/(atk*cthis->fS +1.0f);
    cthis->attrel->iatk = 1.0f - cthis->attrel->atk;
}

void 
tflanger_setEnvelopeRelease(tflanger *cthis, float rls)
{
    cthis->attrel->rls = 1.0f/(rls*cthis->fS +1.0f);
    cthis->attrel->irls = 1.0f - cthis->attrel->rls;
}

void
tflanger_setEnvelopeRateSkew(tflanger *cthis, float skew)
{
    cthis->rateskew = skew;
    if(skew > 100.0) cthis->rateskew = 100.0;
    if(skew < -100.0) cthis->rateskew = -100.0;
}

void 
tflanger_setEnvelopeDepthSkew(tflanger *cthis, float skew)
{
    cthis->depthskew = skew;
    if(skew > 1.0) cthis->depthskew = 1.0;
    if(skew < -1.0) cthis->depthskew = -1.0;
}

void 
tflanger_setEnvelopeWidthSkew(tflanger *cthis, float skew)
{
    cthis->widthskew = skew;
    if(skew > 1.0) cthis->widthskew = 1.0;
    if(skew < -1.0) cthis->widthskew = -1.0;
}

void 
tflanger_setEnvelopeFbSkew(tflanger *cthis, float skew)
{
    cthis->fbskew = skew;
    if(skew > 1.0) cthis->fbskew = 1.0;
    if(skew < -1.0) cthis->fbskew = -1.0;
}


void 
tflanger_setEnvelopeMixSkew(tflanger *cthis, float skew)
{
    cthis->mixskew = skew;
    if(skew > 1.0) cthis->mixskew = 1.0;
    if(skew < -1.0) cthis->mixskew = -1.0;
}

void tflanger_set_lfo_type(tflanger* cthis, unsigned int type) 
{
	set_lfo_type(cthis->lfopar, type);
}

void 
tflanger_setPreset(tflanger *cthis, unsigned int preset)
{
	float ms = 0.001;
	switch(preset)
	{
		case 0: //basic chorus
		    //Set some sane initial values
		    
		    tflanger_setLfoDepth(cthis, (9.5*ms)); //lfo offset, input in seconds
		    tflanger_setLfoWidth(cthis, (1.2*ms)); //lfo deviation, input in seconds will
		                                          //express peak-to-trough deviation
		    tflanger_setLfoRate(cthis, 2.6);    //Input in cycles per second
		    									//LFO Rate synchronized to default delay preset time
		    tflanger_setLfoPhase(cthis, 0.0);    //Phase offset, 0 to 2*pi, useful if 
		                                        //using 2 of these objects in stereo
		    tflanger_setWetDry(cthis, 0.5);     //give ratio wet, 0...1.0. 
		                                        //Dry = 1.0 - fracWet
		    tflanger_setFeedBack(cthis, 0.0);   //Regen
		    tflanger_setTrails(cthis, 0);
			break;
		
		case 1: //basic delay
			tflanger_setLfoDepth(cthis, 385.0*ms);   //delay time
			tflanger_setLfoWidth(cthis, 0.25*ms); //mild modulation
			tflanger_setFeedBack(cthis, 0.36);
			tflanger_setWetDry(cthis, 0.45);
			tflanger_setLfoRate(cthis, 1.3);
			tflanger_setDamping(cthis, 2800.0);  //Similar range to typical BBD delay pedal
			tflanger_setTrails(cthis, 1);
			break;
		
		case 2: //basic flanger 
			tflanger_setLfoDepth(cthis, 0.8*ms);   //sweeps pretty high
			tflanger_setLfoWidth(cthis, 4.0*ms); //mild modulation
			tflanger_setFeedBack(cthis, -0.3);
			tflanger_setWetDry(cthis, -0.5);
			tflanger_setLfoRate(cthis, 0.325); //LFO Rate synchronized to default delay preset time		
			tflanger_setTrails(cthis, 0);
			break;
		
		default:
			break;
	}
}


