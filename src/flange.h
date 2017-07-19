//Linear interpolated delay line

#ifndef TFX_FLANGE_H
#define TFX_FLANGE_H

#include "biquad.h"
#include "lfo.h"

    //First order filter coefficients and state variables
    typedef struct fparams_t {
     float x1;
     float y1;
     float alpha;
     float ialpha;
    } fparams;

    //Attack/Release filter
    typedef struct arparams_t {
         float y1;
         float atk;
         float iatk;
         float rls;
         float irls;
    } arparams;
                  
    typedef struct tflanger_t {
         //Internal Stuff
         float *dlyLine;   //The delay line
         float maxT;       //Time in Seconds -- maximum delay line length
         float fS;         //Sampling Frequency

         float regen;      //Feedback tap

         //The user parameters
         float lfoDepth,lfoDepth0;     //Modulation "zero" delay.  
         float lfoWidth,lfoWidth0;     //Modulation deviation (peak to peak).  LFO applied to Width is always 0.0<LFO<1.0
         float lfoRate;                //Modulation speed
         float lfoPhase;               //Phase to start the LFO.  
         float wet, dry, wet0, dry0;   //User accesses wet, range -1.0 to 1.0.  dry always = 1-abs(wet).
         float feedBack, fb;           //Amount of delayed "wet" signal added back into the input.  -1.0 to 1.0
         float outGain;                //Output level, -10.0 to 10.0
         float envelope_sensitivity;   //gain on envelope detector input
         float rateskew;               //amount of envelope control applied to rate
         float depthskew;              //"" depth
         float widthskew;              //"" width
         float fbskew;                 //"" feedback
         float mixskew;                //"" wet/dry mix
         unsigned char trails;		   // Bypass with trails or not?

         //Delay line pointer
         int dlyWrite;
         int maxDly;
          
         //LFO
         float lfoConst;
         float maxLfoRate;
         lfoparams* lfopar;
         
         //low pass filters
         fparams *envelope;
         arparams *attrel;
         fparams *fader;
         
         //8th order biquad
         biquad_coeffs* f[4];

    } tflanger;
 
     tflanger* tflanger_init (tflanger*, float, float); //provide maximum delay time and the sample rate
     void tflanger_destroy(tflanger*);  //cleanup

     void tflanger_tick(tflanger*, int , float*, float*);  //main processing routine. Returns output in the same buffer 

     void tflanger_setLfoDepth(tflanger*, float); //lfo offset, input in seconds
     void tflanger_setLfoWidth(tflanger*, float); //lfo deviation, input in seconds will express peak-to-trough deviation
     void tflanger_setLfoRate(tflanger*, float);   //Input in cycles per second
     void tflanger_setLfoPhase(tflanger*, float);  //Phase offset, 0 to 2*pi, useful if using 2 of these objects in stereo
     void tflanger_setWetDry(tflanger*, float); //give ratio wet, 0...1.0. 
                                     			//Dry computed from wet 
     void tflanger_setFeedBack(tflanger*, float);  //this can be useful if stereo panning
	 void tflanger_setDamping(tflanger*, float);    // give float frequency 40.0 Hz to 7200.0 Hz
	 
     void tflanger_setFinalGain(tflanger*, float);  //set < 0.99 for partial or complete bypass mode, >=0.99 for normal operation
     void tflanger_setTrails(tflanger*, char);      // Bypass with trails or not?
     
     void tflanger_setEnvelopeAttack(tflanger*, float);
     void tflanger_setEnvelopeRelease(tflanger*, float);
     void tflanger_setEnvelopeSensitivity(tflanger *cthis, float sns);
     void tflanger_setEnvelopeRateSkew(tflanger*, float);
     void tflanger_setEnvelopeDepthSkew(tflanger*, float);
     void tflanger_setEnvelopeWidthSkew(tflanger*, float);
     void tflanger_setEnvelopeFbSkew(tflanger*, float);
     void tflanger_setEnvelopeMixSkew(tflanger*, float);
     void tflanger_set_lfo_type(tflanger*, unsigned int);
     
     void tflanger_setPreset(tflanger*, unsigned int);
     
    //misc filter functions
    float tflanger_atkrls(arparams*, float);         //attack/release
    float tflanger_lpfilter(fparams*, float, char);  //simple first-order low pass
    

    void tflanger_updateParams(tflanger*);
     
  
#endif //TFX_FLANGE_H


