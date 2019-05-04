#include <Bela.h>
#include <stdlib.h>
#include <math.h>

//
//DEBUG
//

#include <Scope.h>
Scope scope;

//
// End DEBUG
//

#include "flange.h"
#include "lfo.h"
#include "inductorwah.h"
#include "reverb.h"
#include "phaser.h"
#include "trem.h"
#include "Sustainer.h"
#include "fb_compressor.h"
#include "eq.h"
#include "envelope_filter.h"
#include "overdrive.h"
#include "klingon.h"

unsigned int gAudioFramesPerAnalogFrame;
//Global input sample rate variable
float gSampleRate = 44100.0;
float gifs = 1.0/gSampleRate;

//DC removal filter parameters
// Not needed with bela core patch
typedef struct dc_remover_t {
	float fs;
	float ifs;
	
	//filter coefficient
	float a;
	
	//State Variables
	float x1, y1;
} dc_remover;
//
//  Effects processing entities
//
    //input processing
dc_remover *ch0_dcr, *ch1_dcr;

    //Graphic equalizers
eq_filters *geq1, *geq2;
    //Delay/echo effect
tflanger* delayline;
	//Chorus
tflanger* chorus;
    //Flanger
tflanger* flanger;
	//Inductor wah modeler
iwah_coeffs* iwah;
int gWahCkt = VOX;
    //Phaser shifter modeled upon analog archetype
phaser_coeffs* phaser;
    //Tremolo effect
trem_coeffs* trem;
    //Guitar Sustainer: Dynamic range compression -- behaves most like a feedback compressor
    //From Rakarrack source repo
Sustainer fx_sustain(44100.0);
	// Overdrive implementation based conceptually on typical guitar stompbox OD like tubescreamer
overdrive* od;
klingon* ko;
	//More explicit feeback compressor implementation
feedback_compressor* fbcompressor;
    //Reverb from Fons Adriaensen, 
    //Documentation of controls valid for this implementation except for ambisonic functions:
    // http://kokkinizita.linuxaudio.org/linuxaudio/zita-rev1-doc/quickguide.html
Reverb zita1;
float gReverb_wet;
float gReverb_mix;

    //Envelope controlled filter based on the state variable filter topology.
    //  TODO:  Also includes LFO and dirty octave effects
env_filter* ef;
float *gMaster_Envelope;

//
// End effects processing entities
//

//Switch context definitions
//Activates controls for the defined effect
#define SW_CTXT_FLANGE			0
#define SW_CTXT_CHORUS			1
#define SW_CTXT_DELAY			2
#define SW_CTXT_WAH				3
#define SW_CTXT_REVERB			4
#define SW_CTXT_PHASE			5
#define SW_CTXT_TREM			6
#define SW_CTXT_SUSTAIN			7
#define SW_CTXT_GEQ1			8
#define SW_CTXT_GEQ2			9
#define SW_CTXT_ENVF			10
#define SW_CTXT_FBCOMP			11
#define SW_CTXT_ODRIVE			12
#define SW_CTXT_KLGN			13
//Update number off effects NFX when adding FX
//Always max in list + 1
#define NFX						14

#define SW_CTXT_NORM			0
#define SW_CTXT_ENV				1
#define SW_CTXT_PG2				1
#define SW_CTXT_PG3				2
#define SW_CTXT_PG4				3

unsigned char gRoundRobin = 0;
unsigned int gScanTimer = 0;

//Time delay effects max delay times
#define T_ECHO		5.0
#define T_FLANGE	0.02
#define T_CHORUS	0.1

//LFO type for effect
unsigned int gDLY_lfo_type;
unsigned int gCHOR_lfo_type;
unsigned int gFLANGE_lfo_type;

//Processing buffer passed from each effect to the next.
float *ch0, *ch1;
int gNframes = 0;

// DEFINES for selection of analog input channel functions

// ------------  Control page context mappings ------------  //
	// Flange, chorus, delay Control page 2
#define LFO_RATE 			0
#define LFO_DEPTH 			1
#define LFO_WIDTH 			2
#define MIX_WET 			3
#define REGEN 				4
#define DAMPING				5

	// Flange, chorus, delay Control page 2
	//     Envelope Control Parameters
#define ENV_SNS_LFO_RATE	0 
#define ENV_SNS_LFO_DEPTH 	1
#define ENV_SNS_LFO_WIDTH 	2
#define ENV_SNS_MIX_WET 	3
#define ENV_SNS_REGEN 		4
//DAMPING uses 5, from above
	//  Flange, chorus, delay Control page 2
#define ENV_SNS				0
#define ENV_ATK				1 
#define ENV_RLS				2

	//wah page 1
#define WAH_CKT				0
#define EXP_PEDAL			7

	//Zita verb Control page 1
#define	REVERB_RTLOW		0
#define REVERB_IDLY			1
#define REVERB_RTMID		2
#define REVERB_MIX			3
#define REVERB_XOV			4
#define REVERB_FDAMP		5

	//Zita Verb Control page 2
#define REVERB_EQL_F		0
#define REVERB_EQH_F		2
#define REVERB_EQL_G		3
#define REVERB_EQH_G		5

	//Tremolo page 1
#define TREMOLO_LFO_TYPE	4

	//Phaser page 1
#define PHASER_STAGES		5

	//Sustainer page 1
#define SUSTAIN_GAIN		0
#define SUSTAIN_DRIVE		1
   
   //Feedback Compressor page 1
#define FB_COMP_THRS		0
#define FB_COMP_RATIO		1
#define FB_COMP_LEVEL		2
#define FB_COMP_MIX			3
#define FB_COMP_ATK			4
#define FB_COMP_RLS			5


	//Envelope filter page 1 extras
#define EF_DET_SNS			5
	//Envelope filter page 2 
#define EF_MIX_LP			0
#define EF_MIX_BP			1
#define EF_MIX_HP			2
#define EF_ATK				3
#define EF_RLS				4
#define EF_DIST				5
	//Envelope filter page 3  -- Envelope gating and sequenced modulator mixing
#define EF_GATE_THRS		0
#define EF_GATE_KNEE		1
#define EF_SH_TRANS			2  //Hard/soft transition when ADSR not active
#define EF_SH_MIX			3	//Mix sample/hold modulator
#define EF_SH_TYPE          5
	//Envelope filter page 4 -- ADSR control
#define EF_ADSR_ATK			0
#define EF_ADSR_DCY			1
#define EF_ADSR_RLS			2
#define EF_ADSR_STN			3

	//Overdrive page 1
#define OD_DRIVE			0
#define OD_TONE				1
#define OD_LEVEL			2
#define OD_BOOST            4

// ----------  End Control page context mappings ----------  //

//For sensing analog inputs as a switch function:
//      Schmitt trigger thresholds
#define SCHMITT_LOW 0.02     //Twist knob all the way down
#define SCHMITT_HIGH 0.98    //Then twist it all the way back up to activate switch state

// Amount of time after twisting knob all the way down before you have to twist it all 
// the way up to get it to activate the switch function
#define KNOB_SWITCH_TIMEOUT 2  //In seconds.  
unsigned int gKnobToggleTimeout = 44100/8;

//channel_filter constants (TODO:  move to its own header and .cpp)
#define CF_CUTOFF 8 //Hz, analog channels filtering cutoff
#define ANALOG_CHANNELS 8  //Actual number of channels to read minus 1 (8 channels - 1)
#define SCAN_PERIOD 50    //ms (used for int).  Time between evaluating whether a control changed
#define N_PBTNS 6

//channel_filter stores values from analog inputs.  Filtering is run on these inputs 
// and stored in the filtered_reading array.
// Also includes state variables for detecting whether there was a position change 
// then a timer to set how long after detecting initial change that it will continue
// to set the value to the active effect defined by control context
// Also maintains state variables for switch function detection.
typedef struct channel_filter_t {
	
	//Store values read from the analog inputs
	float current_reading;
	float filtered_reading[2];
	float *filtered_buf;
	
	//Evaluate whether a control position has changed
	unsigned int scan_cycle;
	unsigned int scan_timer;
	unsigned int active_timer;
	unsigned int active_reset;
	float last_reading;
	float dVdt;
	
	//first order filter coefficients.
	float a;  
	float b;
	float c;
	float d;
	
	//switch function
	bool switch_state;
	bool edge;
	unsigned int knob_toggle_timer;

} channel_filter;

#define T_SW_SCAN 5 //ms
unsigned int gSW_timer;
unsigned int gSW_timer_max;

//State variables for switch debouncing and also switch state logic
typedef struct debounce_t {
	unsigned int integrator;
	unsigned int trigger;
	bool state;
	bool toggle;
	bool rising_edge;
	bool falling_edge;
	
} debouncer;

//State variables to define which effects get updated with values obtained from analog inputs 
// and switches
typedef struct fx_control_context_t {
	//control functions
	unsigned char ain_control_context;
	unsigned char ain_control_context_max;
} fx_control_context;

//Control context 
#define CONTROL_CONTEXT_SWITCH	3  //input 0 appears to be dead on dev's unit (maybe ESD? or just broken wire)
#define CONTROL_EFFECT_SWITCH  	1
#define EFFECT_BYPASS_SWITCH	2
#define WAH_PEDAL_BYPASS		4

// control context switching
fx_control_context fx_cntl[NFX];  //each control stores its context
unsigned char ain_control_effect;
unsigned int startup_mask_timer;

channel_filter knobs[ANALOG_CHANNELS];
debouncer pushbuttons[N_PBTNS];

unsigned int rtprintclock = 0;


/*
*  Apply settings from knobs to delay effect 
*  set_efx_delay_norm() is the first page of settings, which are normal delay effect settings 
*  set_efx_delay_envelope() is the second page of settings, which are envelope control of each parameter
*/
void set_efx_delay_norm()
{
	//Apply settings
	float rate, depth, width, mix, fb, damp;
	rate = depth = width = mix = fb = damp = 0.0;
	bool doprint = false;
	
	if(knobs[LFO_RATE].active_timer != 0) {
		rate = knobs[LFO_RATE].filtered_reading[1];
		rate *= rate;
		rate = map(rate, 0, 1, 0.1, 10.0);
		tflanger_setLfoRate(delayline, rate);
		doprint = true;
	}
	if(knobs[LFO_DEPTH].active_timer != 0) {
		depth = map(knobs[LFO_DEPTH].filtered_reading[1], 0, 1, 0.0005, 1.0);
		tflanger_setLfoDepth(delayline, depth);
		doprint = true;
	}
	if(knobs[LFO_WIDTH].active_timer != 0) {
		width = map(knobs[LFO_WIDTH].filtered_reading[1], 0, 1, 0.0, 0.0025);
		tflanger_setLfoWidth(delayline, width);
		doprint = true;
	}
	if(knobs[MIX_WET].active_timer != 0) {
		if(knobs[MIX_WET].switch_state)
			mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, 0.0, 1.0);
		else
			mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, 0.0, -1.0);
		tflanger_setWetDry(delayline, mix);
		doprint = true;
	}
	if(knobs[REGEN].active_timer != 0) {
		if(knobs[REGEN].switch_state)
			fb = map(knobs[REGEN].filtered_reading[1], 0, 1, 0.0, 1.0);
		else
			fb = map(knobs[REGEN].filtered_reading[1], 0, 1, 0.0, -1.0);
		tflanger_setFeedBack(delayline, fb);
		doprint = true;
	}
	if(knobs[DAMPING].active_timer != 0) {
		damp = knobs[DAMPING].filtered_reading[1];
		damp *= damp;
		damp = map(damp, 0, 1, 40.0, 7400.0);
		tflanger_setDamping(delayline, damp);
		doprint = true;
	}
	
	//doprint=false;
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Delay N Value:  RATE: %f\tDEPTH: %f\tWIDTH: %f\tFB: %f\tMIX: %f\tFB: %f\tDAMP: %f\n", rate, depth, width, mix, fb, damp);
		rtprintclock = 0;
		doprint = true;
	}	
}

void set_efx_delay_envelope()
{
	//Apply settings

	float rate, depth, width, mix, fb, damp;
	rate = depth = width = mix = fb = damp = 0.0;
	bool doprint = false;
	
	if(knobs[ENV_SNS_LFO_RATE].active_timer != 0) {
		rate = map(knobs[LFO_RATE].filtered_reading[1], 0, 1, -10.0, 20.0);
		tflanger_setEnvelopeRateSkew(delayline, rate);
		doprint = true;
	}
	if(knobs[ENV_SNS_LFO_DEPTH].active_timer != 0) {
		depth = map(knobs[LFO_DEPTH].filtered_reading[1], 0, 1, -1.0, 1.0);
		tflanger_setEnvelopeDepthSkew(delayline, depth);
		doprint = true;
	}
	if(knobs[ENV_SNS_LFO_WIDTH].active_timer != 0) {
		width = map(knobs[LFO_WIDTH].filtered_reading[1], 0, 1, -0.0025, 0.0025);
		tflanger_setEnvelopeWidthSkew(delayline, width);
		doprint = true;
	}
	if(knobs[ENV_SNS_MIX_WET].active_timer != 0) {
		mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, -1.0, 1.0);
		tflanger_setEnvelopeMixSkew(delayline, mix);
		doprint = true;
	}
	if(knobs[ENV_SNS_REGEN].active_timer != 0) {
		fb = map(knobs[REGEN].filtered_reading[1], 0, 1, -1.0, 1.0);
		tflanger_setEnvelopeFbSkew(delayline, fb);
		doprint = true;
	}
	if(knobs[DAMPING].active_timer != 0) {
		damp = map(knobs[DAMPING].filtered_reading[1], 0, 1, -1.0, 1.0);
		//tflanger_setEnvelopeDamp(delayline, damp);
		doprint = true;
	}	
	
	
	//doprint=false;
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Delay E Value:  eRATE: %f\teDEPTH: %f\teWIDTH: %f\teMIX: %f\teFB: %f\teDAMP: %f\n", rate, depth, width, mix, fb, damp);
		rtprintclock = 0;
	}	
}

void set_efx_delay_pg3()
{
	//Apply settings

	float sns, attack, release;
	sns = attack = release = 0.0;
	bool doprint = false;
	

	if(knobs[ENV_ATK].active_timer != 0) {
		attack = map(knobs[ENV_ATK].filtered_reading[1], 0, 1, 0.0, 0.25);
		tflanger_setEnvelopeAttack(delayline, attack);
		doprint = true;
	}	
	if(knobs[ENV_RLS].active_timer != 0) {
		release = map(knobs[ENV_RLS].filtered_reading[1], 0, 1, 0.0, 2.0);
		tflanger_setEnvelopeRelease(delayline, release);
		doprint = true;
	}
	if(knobs[ENV_SNS].active_timer != 0) {
		sns = knobs[ENV_SNS].filtered_reading[1];
		sns *= 6.0;
		sns *= sns;
		tflanger_setEnvelopeRelease(delayline, sns);
		doprint = true;
	}
	
	//doprint=false;
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Delay PG3 Value:  SNS: %f\tATK: %f\tRLS: %f\n", sns, attack, release);
		rtprintclock = 0;
	}	
}

void set_efx_delay()
{
	switch(fx_cntl[SW_CTXT_DELAY].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_delay_norm();
		break;
		case SW_CTXT_ENV:
			set_efx_delay_envelope();
		case SW_CTXT_PG3:
			set_efx_delay_pg3();
		break;
	}
}

/*
*  END delay effect settings
*/


/*
*  Apply settings from knobs to flanger effect 
*  set_efx_flange_norm() is the first page of settings, which are normal delay effect settings 
*  set_efx_flange_envelope() is the second page of settings, which are envelope control of each parameter
*/
void set_efx_flange_norm()
{
	//Apply settings
	float rate, depth, width, mix, fb, damp;
	rate = depth = width = mix = fb = damp = 0.0;
	bool doprint = false;
	
	if(knobs[LFO_RATE].active_timer != 0) {
		rate = knobs[LFO_RATE].filtered_reading[1];
		rate *= rate;
		rate = map(rate, 0, 1, 0.1, 10.0);
		tflanger_setLfoRate(flanger, rate);
		doprint = true;
	}
	if(knobs[LFO_DEPTH].active_timer != 0) {
		depth = map(knobs[LFO_DEPTH].filtered_reading[1], 0, 1.0, 0.0, 0.01);
		tflanger_setLfoDepth(flanger, depth);
		doprint = true;
	}
	if(knobs[LFO_WIDTH].active_timer != 0) {
		width = map(knobs[LFO_WIDTH].filtered_reading[1], 0, 1, 0.0, 0.01);
		tflanger_setLfoWidth(flanger, width);
		doprint = true;
	}
	if(knobs[MIX_WET].active_timer != 0) {
		if(knobs[MIX_WET].switch_state)
			mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, 0.0, 1.0);
		else
			mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, 0.0, -1.0);
		tflanger_setWetDry(flanger, mix);
		doprint = true;
	}
	if(knobs[REGEN].active_timer != 0) {
		if(knobs[REGEN].switch_state)
			fb = map(knobs[REGEN].filtered_reading[1], 0, 1, 0.0, 1.0);
		else
			fb = map(knobs[REGEN].filtered_reading[1], 0, 1, 0.0, -1.0);
		tflanger_setFeedBack(flanger, fb);
		doprint = true;
	}
	if(knobs[DAMPING].active_timer != 0) {
		damp = knobs[DAMPING].filtered_reading[1];
		damp *= damp;
		damp = map(damp, 0, 1, 40.0, 7400.0);
		tflanger_setDamping(flanger, damp);
		doprint = true;
	}
	
	//doprint=false;
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Flanger N Value:  FB: %f, %f\tMIX: %f\tDepth:  %f\tDamp: %f\n", knobs[REGEN].current_reading, fb*(2.0-fabs(fb)), mix, depth, damp);
		rtprintclock = 0;
		doprint = true;
	}	
}

void set_efx_flange_envelope()
{
	//Apply settings

	float rate, depth, width, mix, fb, attack, release;
	rate = depth = width = mix = fb = attack = release = 0.0;
	bool doprint = false;
	
	if(knobs[ENV_SNS_LFO_RATE].active_timer != 0) {
		rate = map(knobs[LFO_RATE].filtered_reading[1], 0, 1, -10.0, 20.0);
		tflanger_setEnvelopeRateSkew(flanger, rate);
		doprint = true;
	}
	if(knobs[ENV_SNS_LFO_DEPTH].active_timer != 0) {
		depth = map(knobs[LFO_DEPTH].filtered_reading[1], 0, 1, -0.005, 0.005);
		tflanger_setEnvelopeDepthSkew(flanger, depth);
		doprint = true;
	}
	if(knobs[ENV_SNS_LFO_WIDTH].active_timer != 0) {
		width = map(knobs[LFO_WIDTH].filtered_reading[1], 0, 1, -0.0025, 0.0025);
		tflanger_setEnvelopeWidthSkew(flanger, width);
		doprint = true;
	}
	if(knobs[ENV_SNS_MIX_WET].active_timer != 0) {
		mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, -1.0, 1.0);
		tflanger_setEnvelopeMixSkew(flanger, mix);
		doprint = true;
	}
	if(knobs[ENV_SNS_REGEN].active_timer != 0) {
		fb = map(knobs[REGEN].filtered_reading[1], 0, 1, -1.0, 1.0);
		tflanger_setEnvelopeFbSkew(flanger, fb);
		doprint = true;
	}
	if(knobs[ENV_ATK].active_timer != 0) {
		attack = map(knobs[ENV_ATK].filtered_reading[1], 0, 1, 0.0, 0.5);
		tflanger_setEnvelopeAttack(flanger, attack);
		doprint = true;
	}	
	if(knobs[ENV_RLS].active_timer != 0) {
		release = map(knobs[ENV_RLS].filtered_reading[1], 0, 1, 0.0, 11.0);
		tflanger_setEnvelopeRelease(flanger, release);
		doprint = true;
	}	
	
	doprint=false;
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Flanger E Value:  FB: %f\tMIX: %f\tDepth:  %f\tATK: %f\tRLS: %f\n", fb, mix ,depth, attack, release);
		rtprintclock = 0;
		doprint = true;
	}	
}

void set_efx_flange()
{
	switch(fx_cntl[SW_CTXT_FLANGE].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_flange_norm();
		break;
		case SW_CTXT_ENV:
			set_efx_flange_envelope();
		break;
	}
}

/*
*  END flange effect settings
*/


/*
*  Apply settings from knobs to chorus effect 
*  set_efx_chorus_norm() is the first page of settings, which are normal delay effect settings 
*  set_efx_chorus_envelope() is the second page of settings, which are envelope control of each parameter
*/
void set_efx_chorus_norm()
{
	//Apply settings
	float rate, depth, width, mix, fb, damp;
	rate = depth = width = mix = fb = damp = 0.0;
	bool doprint = false;
	
	if(knobs[LFO_RATE].active_timer != 0) {
		rate = knobs[LFO_RATE].filtered_reading[1];
		rate *= rate;
		rate = map(rate, 0, 1, 0.1, 10.0);
		tflanger_setLfoRate(chorus, rate);
		doprint = true;
	}
	if(knobs[LFO_DEPTH].active_timer != 0) {
		depth = map(knobs[LFO_DEPTH].filtered_reading[1], 0, 1.0, 0.005, 0.035);
		tflanger_setLfoDepth(chorus, depth);
		doprint = true;
	}
	if(knobs[LFO_WIDTH].active_timer != 0) {
		width = map(knobs[LFO_WIDTH].filtered_reading[1], 0, 1, 0.0, 0.005);
		tflanger_setLfoWidth(chorus, width);
		doprint = true;
	}
	if(knobs[MIX_WET].active_timer != 0) {
		if(knobs[MIX_WET].switch_state)
			mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, 0.0, 1.0);
		else
			mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, 0.0, -1.0);
		tflanger_setWetDry(chorus, mix);
		doprint = true;
	}
	if(knobs[REGEN].active_timer != 0) {
		if(knobs[REGEN].switch_state)
			fb = map(knobs[REGEN].filtered_reading[1], 0, 1, 0.0, 1.0);
		else
			fb = map(knobs[REGEN].filtered_reading[1], 0, 1, 0.0, -1.0);
		tflanger_setFeedBack(chorus, fb);
		doprint = true;
	}
	if(knobs[DAMPING].active_timer != 0) {
		damp = knobs[DAMPING].filtered_reading[1];
		damp *= damp;
		damp = map(damp, 0, 1, 40.0, 7400.0);
		tflanger_setDamping(chorus, damp);
		doprint = true;
	}
	
	//doprint=false;
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Chorus N Value:  FB: %f, %f\tMIX: %f\tDepth:  %f\tDamp: %f\n", knobs[REGEN].current_reading, fb*(2.0-fabs(fb)), mix, depth, damp);
		rtprintclock = 0;
		doprint = true;
	}	
}

void set_efx_chorus_envelope()
{
	//Apply settings

	float rate, depth, width, mix, fb, attack, release;
	rate = depth = width = mix = fb = attack = release = 0.0;
	bool doprint = false;
	
	if(knobs[ENV_SNS_LFO_RATE].active_timer != 0) {
		rate = map(knobs[LFO_RATE].filtered_reading[1], 0, 1, -10.0, 20.0);
		tflanger_setEnvelopeRateSkew(chorus, rate);
		doprint = true;
	}
	if(knobs[ENV_SNS_LFO_DEPTH].active_timer != 0) {
		depth = map(knobs[LFO_DEPTH].filtered_reading[1], 0, 1, -0.005, 0.005);
		tflanger_setEnvelopeDepthSkew(chorus, depth);
		doprint = true;
	}
	if(knobs[ENV_SNS_LFO_WIDTH].active_timer != 0) {
		width = map(knobs[LFO_WIDTH].filtered_reading[1], 0, 1, -0.005, 0.005);
		tflanger_setEnvelopeWidthSkew(chorus, width);
		doprint = true;
	}
	if(knobs[ENV_SNS_MIX_WET].active_timer != 0) {
		mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, -1.0, 1.0);
		tflanger_setEnvelopeMixSkew(chorus, mix);
		doprint = true;
	}
	if(knobs[ENV_SNS_REGEN].active_timer != 0) {
		fb = map(knobs[REGEN].filtered_reading[1], 0, 1, -1.0, 1.0);
		tflanger_setEnvelopeFbSkew(chorus, fb);
		doprint = true;
	}
	if(knobs[ENV_ATK].active_timer != 0) {
		attack = map(knobs[ENV_ATK].filtered_reading[1], 0, 1, 0.01, 0.5);
		tflanger_setEnvelopeAttack(chorus, attack);
		doprint = true;
	}	
	if(knobs[ENV_RLS].active_timer != 0) {
		release = map(knobs[ENV_RLS].filtered_reading[1], 0, 1, 0.01, 11.0);
		tflanger_setEnvelopeRelease(chorus, release);
		doprint = true;
	}	
	
	doprint=false;
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Chorus E Value:  FB: %f\tMIX: %f\tDepth:  %f\tATK: %f\tRLS: %f\n", fb, mix ,depth, attack, release);
		rtprintclock = 0;
		doprint = true;
	}	
}

void set_efx_chorus()
{
	switch(fx_cntl[SW_CTXT_CHORUS].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_chorus_norm();
		break;
		case SW_CTXT_ENV:
			set_efx_chorus_envelope();
		break;
	}
}

/*
*  END chorus effect settings
*/

/*
*  Apply settings from knobs to wah effect 
*  set_efx_wah_norm() is the first page of settings, which are normal delay effect settings 
*  set_efx_wah_envelope() is the second page of settings, which are envelope control of each parameter
*/
void set_efx_wah_norm()
{
	bool doprint = false;
	float tmp = 0.0;
	if(knobs[WAH_CKT].active_timer != 0) {
		tmp = knobs[WAH_CKT].filtered_reading[1]*MAX_WAHS;
		gWahCkt = lrintf(floorf(tmp));
		iwah_circuit_preset(gWahCkt, iwah, gSampleRate );
		doprint = true;
	}
	

	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Wah Circuit: %d\n", gWahCkt);
		rtprintclock = 0;
		doprint = true;
	}
}

void set_efx_wah_envelope()
{
	
}

void set_efx_wah()
{
	switch(fx_cntl[SW_CTXT_WAH].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_wah_norm();
			break;
		case SW_CTXT_ENV:
			set_efx_wah_envelope();
			break;
	}	
}
/*
*  END wah effect settings
*/

/*
*  Apply settings from knobs to overdrive effect 
*  set_efx_overdrive_norm() is the first page of settings, which are normal effect control settings 
*/
void set_efx_overdrive_norm()
{
	//Apply settings

	float drive, tone, level;
	drive = tone = level =  0.0;

	if(knobs[OD_DRIVE].active_timer != 0) {
		drive = map(knobs[OD_DRIVE].filtered_reading[1], 0, 1, 12.0, 45.0);
		od_set_drive(od, drive);
		
		if(rtprintclock >= 20000)
		{
			rt_printf("Overdrive: DRIVE: %f\n", drive);
			rtprintclock = 0;
		}
		
	}
	if(knobs[OD_TONE].active_timer != 0) {
		tone = map(knobs[OD_TONE].filtered_reading[1], 0, 1, -12.0, 12.0);
		od_set_tone(od, tone);

		if(rtprintclock >= 20000)
		{
			rt_printf("Overdrive: TONE: %f\n", tone);
			rtprintclock = 0;
		}
		
	}
	if(knobs[OD_LEVEL].active_timer != 0) {
		level = map(knobs[OD_LEVEL].filtered_reading[1], 0, 1, -40.0, 6.0);
		od_set_level(od, level);

		if(rtprintclock >= 20000)
		{
			rt_printf("Overdrive: LEVEL:  %f\n", level);
			rtprintclock = 0;
		}
	}
}


void set_efx_overdrive()
{
	switch(fx_cntl[SW_CTXT_ODRIVE].ain_control_context)
	{
		case SW_CTXT_ENV:
		case SW_CTXT_NORM:
			set_efx_overdrive_norm();
			break;

	}	
}
/*
*  END overdrive effect settings
*/


/*
*  Apply settings from knobs to klingon-tone overdrive effect 
*  set_efx_klingon_norm() is the first page of settings, which are normal effect control settings 
*/
void set_efx_klingon_norm()
{
	//Apply settings

	float drive, tone, level, boost;
	drive = tone = level = boost = 0.0;

	if(knobs[OD_DRIVE].active_timer != 0) {
		drive = map(knobs[OD_DRIVE].filtered_reading[1], 0, 1, 12.0, 45.0);
		kot_set_drive(ko, drive);
		
		if(rtprintclock >= 20000)
		{
			rt_printf("Klingon: DRIVE: %f\n", drive);
			rtprintclock = 0;
		}
		
	}
	if(knobs[OD_TONE].active_timer != 0) {
		tone = map(knobs[OD_TONE].filtered_reading[1], 0, 1, -40.0, 0.0);
		kot_set_tone(ko, tone);

		if(rtprintclock >= 20000)
		{
			rt_printf("Klingon: TONE: %f\n", tone);
			rtprintclock = 0;
		}
		
	}
	if(knobs[OD_LEVEL].active_timer != 0) {
		level = map(knobs[OD_LEVEL].filtered_reading[1], 0, 1, -40.0, 0.0);
		kot_set_level(ko, level);

		if(rtprintclock >= 20000)
		{
			rt_printf("Klingon: LEVEL:  %f\n", level);
			rtprintclock = 0;
		}
	}
		if(knobs[OD_BOOST].active_timer != 0) {
		boost = map(knobs[OD_BOOST].filtered_reading[1], 0, 1, 0.0, 1.0);
		kot_set_boost(ko, boost);

		if(rtprintclock >= 20000)
		{
			rt_printf("Klingon: BOOST:  %f\n", boost);
			rtprintclock = 0;
		}
	}
}


void set_efx_klingon()
{
	switch(fx_cntl[SW_CTXT_KLGN].ain_control_context)
	{
		case SW_CTXT_ENV:
		case SW_CTXT_NORM:
			set_efx_klingon_norm();
			break;

	}	
}
/*
*  END klingon effect settings
*/

/*
*  Apply settings from knobs to Zita reverb effect 
*  No envelope controls currently implemented for reverb, but functions exist as placeholders.
*/
void set_efx_reverb_norm()
{
	float opmix, xov, rtlo, rtmid, fdamp, eql, eqh, idly;
	xov = rtlo = rtmid = fdamp = eql = eqh = idly = opmix = 0.0;
	bool doprint = false;
	
	if(knobs[REVERB_MIX].active_timer != 0) 
	{
		// gReverb_mix = knobs[REVERB_MIX].filtered_reading[1];
		// if(gReverb_wet > 0.0) gReverb_wet = gReverb_mix;
		
		opmix = knobs[REVERB_MIX].filtered_reading[1];
		zita1.set_opmix (opmix);
		doprint = true;
	}
	else if(knobs[REVERB_XOV].active_timer != 0) 
	{
		xov = map(knobs[REVERB_XOV].filtered_reading[1], 0.0, 1.0, 50.0, 1000.0);
		zita1.set_xover(xov);
		doprint = true;
	}
	else if(knobs[REVERB_RTLOW].active_timer != 0) 
	{
		rtlo = map(knobs[REVERB_RTLOW].filtered_reading[1], 0.0, 1.0, 1.0, 8.0);
		zita1.set_rtlow(rtlo);
		doprint = true;
	}	
	else if(knobs[REVERB_RTMID].active_timer != 0) 
	{
		rtmid = map(knobs[REVERB_RTMID].filtered_reading[1], 0.0, 1.0, 1.0, 8.0);
		zita1.set_rtmid(rtmid);
		doprint = true;
	}
	else if(knobs[REVERB_IDLY].active_timer != 0) 
	{
		idly = map(knobs[REVERB_IDLY].filtered_reading[1], 0.0, 1.0, 0.02, 0.1);
		zita1.set_delay(idly);
		doprint = true;
	}	
	else if(knobs[REVERB_FDAMP].active_timer != 0) 
	{
		fdamp = knobs[REVERB_FDAMP].filtered_reading[1];
		fdamp = 1500.0 + 20000.0*fdamp*fdamp; //poor-man's log taper
		zita1.set_fdamp(fdamp);
		doprint = true;
	}
	
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Zita Reverb: %f\n", gReverb_mix);
		rtprintclock = 0;
	}
}

void set_efx_reverb_pg2()
{
	float eqlf, eqlg, eqhf, eqhg;
	eqlf = eqlg = eqhf = eqhg = 0.0;
	bool doprint = false;
	
	if( (knobs[REVERB_EQL_F].active_timer != 0) || (knobs[REVERB_EQL_G].active_timer != 0) )
	{
		eqlf = knobs[REVERB_EQL_F].filtered_reading[1];
		eqlf = 40.0 + 460.0*eqlf*eqlf;
		eqlg = map(knobs[REVERB_EQL_G].filtered_reading[1], 0.0, 1.0, -12.0, 12.0);
		zita1.set_eq1(eqlf, eqlg);
		doprint = true;
	}
	else if( (knobs[REVERB_EQH_F].active_timer != 0) || (knobs[REVERB_EQH_G].active_timer != 0) )
	{
		eqhf = knobs[REVERB_EQH_F].filtered_reading[1];
		eqhf = 750.0 + 10000.0*eqlf*eqlf;
		eqhg = map(knobs[REVERB_EQH_G].filtered_reading[1], 0.0, 1.0, -12.0, 12.0);
		zita1.set_eq2(eqhf, eqhg);
		doprint = true;
	}
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("EQLF: %f\n", eqlf);
		rtprintclock = 0;
	}	
}

void set_efx_reverb()
{
	switch(fx_cntl[SW_CTXT_REVERB].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_reverb_norm();
			break;
		case SW_CTXT_ENV:
			set_efx_reverb_pg2();
			break;
	}
}

/*
*  END Reverb effect settings
*/

//
// Phaser settings
//

void set_efx_phaser_norm()
{
	//Apply settings

	float rate, depth, width, mix, fb;
	int ns;
	rate = depth = width = mix = fb = 0.0;
	ns = 0;
	bool doprint = false;
	
	if(knobs[LFO_RATE].active_timer != 0) {
		rate = knobs[LFO_RATE].filtered_reading[1];
		rate *= rate;
		rate = map(rate, 0, 1, 0.05, 10.0);
		phaser_set_lfo_rate(phaser, rate);
		doprint = true;
	}
	if(knobs[LFO_DEPTH].active_timer != 0) {
		depth = knobs[LFO_DEPTH].filtered_reading[1];
		depth *= depth;
		depth = map(depth, 0, 1, 100.0, 1000.0);
		phaser_set_lfo_depth(phaser, depth, 0);
		doprint = true;
	}
	if(knobs[LFO_WIDTH].active_timer != 0) {
		width = knobs[LFO_WIDTH].filtered_reading[1];
		width *= width;
		width = map(width, 0, 1, 0.0, 5000.0);
		phaser_set_lfo_width(phaser, width, 0);
		doprint = true;
	}
	if(knobs[MIX_WET].active_timer != 0) {
		mix =  map(knobs[MIX_WET].filtered_reading[1], 0, 1, -1.0, 1.0);
		phaser_set_mix(phaser, mix);
		doprint = true;
	}
	if(knobs[REGEN].active_timer != 0) {
		fb = map(knobs[REGEN].filtered_reading[1], 0, 1, -1.0, 1.0);
		phaser_set_feedback(phaser, fb, 3);
		doprint = true;
	}
	if(knobs[PHASER_STAGES].active_timer != 0) {
		ns = lrintf( map(knobs[PHASER_STAGES].filtered_reading[1], 0, 1, 2.0, 24.0) );
		phaser_set_nstages(phaser, ns);
		doprint = true;
	}

	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Phaser N Value:  RATE: %f\t DEPTH: %f\tWIDTH: %f\tMIX: %f\tFB:  %f\tNSTAGES: %d\n", rate, depth, width, mix, fb, ns);
		rtprintclock = 0;
	}		
}

void set_efx_phaser_pg2()
{
	
}

void set_efx_phase()
{
	switch(fx_cntl[SW_CTXT_PHASE].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_phaser_norm();
			break;
		case SW_CTXT_ENV:
			set_efx_phaser_pg2();
			break;
	}
}

/*
*  End Phaser effect
*/

/*
*  Tremolo Effect Settings
*/
void set_efx_trem_norm()
{

	float rate, depth, width, ftype;
	unsigned int lfo_type = 0;
	char lfo_name[100];
	rate = depth = width = ftype = 0.0;
	bool doprint = false;
	
	if(knobs[LFO_RATE].active_timer != 0) {
		rate = knobs[LFO_RATE].filtered_reading[1];
		rate *= rate;
		rate = map(rate, 0, 1, 0.05, 20.0);
		trem_set_lfo_rate(trem, rate);
		doprint = true;
	}
	if(knobs[LFO_DEPTH].active_timer != 0) {
		depth = knobs[LFO_DEPTH].filtered_reading[1];
		depth *= depth;
		depth = map(depth, 0, 1, 1.0, 2.0);
		trem_set_lfo_gain(trem, depth);
		doprint = true;
	}
	if(knobs[LFO_WIDTH].active_timer != 0) {
		width = knobs[LFO_WIDTH].filtered_reading[1];
		width = map(width, 0, 1, 0.33, 1.0);
		trem_set_lfo_depth(trem, width);
		doprint = true;
	}
	if(knobs[TREMOLO_LFO_TYPE].active_timer != 0) {
		ftype = knobs[TREMOLO_LFO_TYPE].filtered_reading[1];
		ftype = floorf(ftype*(MAX_LFOS + 1.0));
		lfo_type = lrintf(ftype);
		trem_set_lfo_type(trem, lfo_type);
		doprint = true;
	}

	if( (rtprintclock >= 20000) && (doprint) ){
		get_lfo_name(lfo_type, lfo_name);
		rt_printf("Tremolo N Value:  RATE: %f\t GAIN: %f\tDEPTH: %f\t%d:\t%s\n", rate, depth, width, lfo_type, lfo_name);
		rtprintclock = 0;
	}
}

void set_efx_trem_pg2()
{
	
}

void set_efx_trem()
{
	switch(fx_cntl[SW_CTXT_TREM].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_trem_norm();
			break;
		case SW_CTXT_ENV:
			set_efx_trem_pg2();
			break;
	}
}

/*
*  End Tremolo effect
*/


/*
*  Sustainer Effect Settings
*/
void set_efx_sustainer_norm()
{

	float sustain, gain;

	gain = sustain = 1.0;
	bool doprint = false;
	
	if(knobs[SUSTAIN_GAIN].active_timer != 0) {
		gain = knobs[SUSTAIN_GAIN].filtered_reading[1];
		fx_sustain.setGain(gain);
		doprint = true;
	}
	if(knobs[SUSTAIN_DRIVE].active_timer != 0) {
		sustain = knobs[SUSTAIN_DRIVE].filtered_reading[1];
		fx_sustain.setSustain(sustain);
		doprint = true;
	}

	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Sustainer N Value:  SUSTAIN: %f\t GAIN: %f\n", sustain, gain);
		rtprintclock = 0;
	}
}

void set_efx_sustainer_pg2()
{

	
}

void set_efx_sustainer()
{
	switch(fx_cntl[SW_CTXT_SUSTAIN].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_sustainer_norm();
			break;
		case SW_CTXT_ENV:
			set_efx_sustainer_pg2();
			break;
	}
}

/*
*  End Sustainer effect
*/

/*
*  Graphic equalizers
*/

void set_efx_geq1_norm()
{
	float g = 0.0;
	int i = 0;
	bool doprint = false;
	int band = 0;
	
	for(i=0; i < geq1->nbands; i++)
	{	
		if(knobs[i].active_timer != 0) {
			g = map(knobs[i].filtered_reading[1], 0.0, 1.0, -12.0, 12.0);
			eq_update_gain(geq1->band[i], g);
			doprint = true;
			band = i;
		}
		
	}
	
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("GEQ1 N Value:  GAIN: %f\t BAND: %d\n", g, band);
		rtprintclock = 0;
	}
	
}

void set_efx_geq1_pg2()
{
	//set high and low shelving filters
	float g = 0.0;
	bool doprint = false;
	int band = 0;
	if(knobs[0].active_timer != 0) 
	{
		g = map(knobs[0].filtered_reading[1], 0.0, 1.0, -12.0, 12.0);
		eq_update_gain(geq1->band[geq1->nbands], g);
		doprint = true;
	}
	else if(knobs[1].active_timer != 0) 
	{
		g = map(knobs[1].filtered_reading[1], 0.0, 1.0, -12.0, 12.0);
		eq_update_gain(geq1->band[geq1->nbands + 1], g);
		doprint = true;
		band = 1;
	}

	if( (rtprintclock >= 20000) && (doprint) ){
		if(band == 0)
			rt_printf("GEQ1 PG2 Value:  GAIN: %f\t BAND: LOW_SHELF\n", g);
		else
			rt_printf("GEQ1 PG2 Value:  GAIN: %f\t BAND: HIGH_SHELF\n", g);
			
		rtprintclock = 0;
	}	
}

void set_efx_geq1()
{
	switch(fx_cntl[SW_CTXT_GEQ1].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_geq1_norm();
			break;
		case SW_CTXT_ENV:
			set_efx_geq1_pg2();
			break;
	}	
}

void set_efx_geq2()
{
	if( rtprintclock >= 40000 ){
		rt_printf("TODO : POST EQ IS UNIMPLEMENTED\n");
		rtprintclock = 0;
	}
}

/*
*  End Graphic Equalizers
*/

/*
*  Envelope Filter
*/
void set_efx_envf_norm()
{
	//Apply settings

	float rate, depth, width, mix, q, sns;
	rate = depth = width = mix = q = sns = 0.0;
	bool doprint = false;
	
	if(knobs[LFO_RATE].active_timer != 0) {
		//TODO:  UNIMPLEMENTED
		rate = knobs[LFO_RATE].filtered_reading[1];
		rate *= rate;
		rate = map(rate, 0.0, 1.0, 0.1, 30.0);
		envf_set_lfo_rate(ef, rate);
		doprint = true;
	}
	if(knobs[LFO_DEPTH].active_timer != 0) {
		depth = knobs[LFO_DEPTH].filtered_reading[1];
		depth *= depth;
		depth = map(depth, 0.0, 1.0, 10.0, 1000.0);
		envf_set_depth(ef, depth);
		doprint = true;
	}
	if(knobs[LFO_WIDTH].active_timer != 0) {
		width = knobs[LFO_WIDTH].filtered_reading[1];
		width *= width;
		width = map(width, 0.0, 1.0, 0.0, 5000.0);
		envf_set_width(ef, width);
		doprint = true;
	}
	if(knobs[MIX_WET].active_timer != 0) {
		mix =  map(knobs[MIX_WET].filtered_reading[1], 0.0, 1.0, -1.0, 1.0);
		envf_set_mix(ef, mix);
		doprint = true;
	}
	if(knobs[REGEN].active_timer != 0) {
		q = knobs[REGEN].filtered_reading[1];
		q *= q;
		q = map(q, 0.0, 1.0, 0.5, 60.0);
		envf_set_q(ef, q);
		doprint = true;
	}
	if(knobs[EF_DET_SNS].active_timer != 0) {
		sns = knobs[EF_DET_SNS].filtered_reading[1];
		sns *= sns;
		sns = map(sns, 0.0, 1.0, -3.0, 3.0);
		envf_set_sensitivity(ef, sns);
		doprint = true;
	}
	

	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Envelope Filter N Value:  RATE: %f\t DEPTH: %f\tWIDTH: %f\tMIX: %f\tQ:  %f\tSNS: %f\n", rate, depth, width, mix, q, sns);
		rtprintclock = 0;
	}	
}

void set_efx_envf_pg2()
{
	bool doprint = false;
	float hmix, bmix, lmix, atk, rls, dist;
	hmix = bmix = lmix = atk = rls = dist = 0.0;

	if(knobs[EF_MIX_LP].active_timer != 0) {
		lmix = map(knobs[EF_MIX_LP].filtered_reading[1], 0.0, 1.0, -1.0, 1.0);
		envf_set_mix_lpf(ef, lmix);
		doprint = true;
	}
	if(knobs[EF_MIX_BP].active_timer != 0) {
		bmix = map(knobs[EF_MIX_BP].filtered_reading[1], 0.0, 1.0, -1.0, 1.0);
		envf_set_mix_bpf(ef, bmix);
		doprint = true;
	}
	if(knobs[EF_MIX_HP].active_timer != 0) {
		hmix = map(knobs[EF_MIX_HP].filtered_reading[1], 0.0, 1.0, -1.0, 1.0);
		envf_set_mix_hpf(ef, hmix);
		doprint = true;
	}
	if(knobs[EF_ATK].active_timer != 0) {
		atk = map(knobs[EF_ATK].filtered_reading[1], 0.0, 1.0, 0.001, 0.25);
		envf_set_atk(ef, atk);
		doprint = true;
	}
	if(knobs[EF_RLS].active_timer != 0) {
		rls = map(knobs[EF_RLS].filtered_reading[1], 0.0, 1.0, 0.01, 2.0);
		envf_set_rls(ef, rls);
		doprint = true;
	}
	if(knobs[EF_DIST].active_timer != 0) {
		dist = map(knobs[EF_DIST].filtered_reading[1], 0.0, 1.0, 0.001, 2.0);
		envf_set_drive(ef, dist);
		doprint = true;
	}
	
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Envelope Filter Pg2:  MIX_HPF: %f\tMIX_BPF: %f\tMIX_LPF: %f\tATK: %f\tRLS:  %f\tDRV: %f\n", hmix, bmix, lmix, atk, rls, dist);
		rtprintclock = 0;
	}
}

void set_efx_envf_pg3()
{
	float gate_thrs, gate_knee, shmix, sht;
	int nsht;
	bool doprint = false;
	gate_knee = gate_thrs = shmix = 0.0;
	
	if(knobs[EF_GATE_THRS].active_timer != 0) {
		gate_thrs = map(knobs[EF_GATE_THRS].filtered_reading[1], 0.0, 1.0, -60.0, -12.0);
		envf_set_gate(ef, gate_thrs);
		doprint = true;
	}
	if(knobs[EF_GATE_KNEE].active_timer != 0) {
		gate_knee = map(knobs[EF_GATE_KNEE].filtered_reading[1], 0.0, 1.0, 0.1, 12.0);
		envf_set_gate_knee(ef, gate_knee);
		doprint = true;
	}
	if(knobs[EF_SH_MIX].active_timer != 0) {
		shmix = knobs[EF_SH_MIX].filtered_reading[1];
		envf_set_mix_sh_modulator(ef, shmix);
		doprint = true;
	}
	if(knobs[EF_SH_TYPE].active_timer != 0) {
		sht = knobs[EF_SH_TYPE].filtered_reading[1];
		nsht = lrintf(sht*(ef->sh->max_types - 1.0));
		envf_set_sample_hold_type(ef, nsht);
		doprint = true;
	}
	//Select whether ADSR is active
	if(knobs[EF_SH_TYPE].edge)
	{
		knobs[EF_SH_TYPE].edge = false;
		
		if( envf_set_adsr_active(ef, true) == true )
		{
			rt_printf("ACTIVATED ADSR for filter Sample/Hold Effect\n");	
		} else
		{
			rt_printf("DISABLED ADSR for filter Sample/Hold Effect\n");
		}
	}
	
	
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Envelope Filter Pg3:  GATE_THRESH: %f\tGATE_KNEE: %f\tSHMIX: %f\tSHTYPE: %d\n", gate_thrs, gate_knee, shmix, nsht);
		rtprintclock = 0;
	}	
}

void set_efx_envf_pg4()
{
	float atk, dcy, stn, rls;
	bool doprint = false;
	atk = dcy = stn = rls = 0.0;
	
	if(knobs[EF_ADSR_ATK].active_timer != 0) {
		atk = knobs[EF_ADSR_ATK].filtered_reading[1];
		atk *= atk*1000.0;
		envf_set_adsr_atk(ef, atk);
		doprint = true;
	}
	if(knobs[EF_ADSR_RLS].active_timer != 0) {
		rls = knobs[EF_ADSR_RLS].filtered_reading[1];
		rls *= rls*1000.0;
		envf_set_adsr_rls(ef, rls);
		doprint = true;
	}
	if(knobs[EF_ADSR_DCY].active_timer != 0) {
		dcy = knobs[EF_ADSR_DCY].filtered_reading[1];
		dcy *= dcy*1000.0;
		envf_set_adsr_dcy(ef, dcy);
		doprint = true;
	}
	if(knobs[EF_ADSR_STN].active_timer != 0) {
		stn = knobs[EF_ADSR_STN].filtered_reading[1];
		stn *= stn;
		envf_set_adsr_stn(ef, stn);
		doprint = true;
	} 
	
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("Envelope Filter Pg4:  EF_ADSR_ATK: %f\EF_ADSR_RLS: %f\tEF_ADSR_STN: %f\tEF_ADSR_RLS: %f\n", atk, dcy, stn, rls);
		rtprintclock = 0;
	}	
}

void set_efx_envf()
{
	switch(fx_cntl[SW_CTXT_ENVF].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_envf_norm();
			break;
		case SW_CTXT_PG2:
			set_efx_envf_pg2();
			break;
		case SW_CTXT_PG3:
			set_efx_envf_pg3();
			break;
		case SW_CTXT_PG4:
			set_efx_envf_pg4();
			break;
	}	
}


/*
*  End Envelope Filter
*/

/*
*  Feedback compressor config
*/ 
void set_efx_fbcomp_norm()
{
	bool doprint = false;
	float thresh, atk, rls, ratio, olevel, wet;
	thresh = atk = rls = ratio = olevel = wet = 0.0;

	if(knobs[FB_COMP_THRS].active_timer != 0) {
		thresh = map(knobs[FB_COMP_THRS].filtered_reading[1], 0.0, 1.0, -60.0, -12.0);
		feedback_compressor_set_threshold(fbcompressor, thresh);
		doprint = true;
	}	
	if(knobs[FB_COMP_RATIO].active_timer != 0) {
		ratio = map(knobs[FB_COMP_RATIO].filtered_reading[1], 0.0, 1.0, 1.0, 24.0);
		feedback_compressor_set_ratio(fbcompressor, ratio);
		doprint = true;
	}	
	if(knobs[FB_COMP_LEVEL].active_timer != 0) {
		olevel = map(knobs[FB_COMP_LEVEL].filtered_reading[1], 0.0, 1.0, -24.0, 6.0);
		feedback_compressor_set_out_gain(fbcompressor, olevel);
		doprint = true;
	}	
	if(knobs[FB_COMP_MIX].active_timer != 0) {
		wet = knobs[FB_COMP_MIX].filtered_reading[1];
		feedback_compressor_set_mix(fbcompressor, wet);
		doprint = true;
	}
	if(knobs[FB_COMP_ATK].active_timer != 0) {
		atk = knobs[FB_COMP_ATK].filtered_reading[1];
		atk *= atk;
		atk = map(knobs[FB_COMP_ATK].filtered_reading[1], 0.0, 1.0, 1.0, 1000.0);
		feedback_compressor_set_attack(fbcompressor, atk);
		doprint = true;
	}
	if(knobs[FB_COMP_RLS].active_timer != 0) {
		rls = knobs[FB_COMP_RLS].filtered_reading[1];
		rls *= rls;
		rls = map(rls, 0.0, 1.0, 20.0, 1000.0);
		feedback_compressor_set_release(fbcompressor, rls);
		doprint = true;
	}	
	
	if( (rtprintclock >= 20000) && (doprint) ){
		rt_printf("FB COMP PG1:  FB_COMP_THRS: %f\tFB_COMP_ATK: %f\tFB_COMP_RLS: %f\tFB_COMP_RATIO: %f\tFB_COMP_LEVEL: %f\tFB_COMP_MIX: %f\n", thresh, atk, rls, ratio, olevel, wet);
		rtprintclock = 0;
	}	
}

void set_efx_fbcomp()
{
	switch(fx_cntl[SW_CTXT_FBCOMP].ain_control_context)
	{
		case SW_CTXT_NORM:
			set_efx_fbcomp_norm();
			break;
		default:
			set_efx_fbcomp_norm();
			break;
	}
}

/*
*  End Feedback Compressor
*/

void apply_settings()
{
	if(startup_mask_timer > 0) return;
	
	//Decide who gets analog inputs 
	switch(ain_control_effect)
	{
		case SW_CTXT_FLANGE :
			set_efx_flange();
			if(knobs[LFO_RATE].edge) {
				knobs[LFO_RATE].edge = false;
				if(++gFLANGE_lfo_type > MAX_LFOS)
					gFLANGE_lfo_type = 0;
				tflanger_set_lfo_type(delayline, gFLANGE_lfo_type);
				char outstr[30];
				get_lfo_name(gFLANGE_lfo_type, outstr);
				rt_printf("Changed LFO to %s\n", outstr);
			}
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(flanger->outGain > 0.0) {
						tflanger_setFinalGain(flanger, 0.0);
						rt_printf("Flanger Effect BYPASSED\n");
					}
					else
					{
						tflanger_setFinalGain(flanger, 1.0);
						rt_printf("Flanger Effect ENABLED\n");
					}
					
			}			
			break;

		case SW_CTXT_CHORUS :
			set_efx_chorus();
			if(knobs[LFO_RATE].edge) {
				knobs[LFO_RATE].edge = false;
				if(++gCHOR_lfo_type > MAX_LFOS)
					gCHOR_lfo_type = 0;
				tflanger_set_lfo_type(delayline, gCHOR_lfo_type);
				char outstr[30];
				get_lfo_name(gCHOR_lfo_type, outstr);
				rt_printf("Changed LFO to %s\n", outstr);
			}
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(chorus->outGain > 0.0) {
						tflanger_setFinalGain(chorus, 0.0);
						rt_printf("Chorus Effect BYPASSED\n");
					}
					else
					{
						tflanger_setFinalGain(chorus, 1.0);
						rt_printf("Chorus Effect ENABLED\n");
					}
					
			}		
			break;

		case SW_CTXT_DELAY :
			set_efx_delay();
			if(knobs[LFO_RATE].edge)
			{
				knobs[LFO_RATE].edge = false;
				if(++gDLY_lfo_type > MAX_LFOS)
					gDLY_lfo_type = 0;
				tflanger_set_lfo_type(delayline, gDLY_lfo_type);
				char outstr[30];
				get_lfo_name(gDLY_lfo_type, outstr);
				rt_printf("Changed LFO to %s\n", outstr);
				
			}
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(delayline->outGain >= 1.0) {
						tflanger_setFinalGain(delayline, 0.0);
						rt_printf("Delay Effect BYPASSED\n");
					}
					else
					{
						tflanger_setFinalGain(delayline, 1.0);
						rt_printf("Delay Effect ENABLED\n");
					}
					
			}
			break;
		case SW_CTXT_WAH :
			set_efx_wah();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(iwah->bypass) {
						iwah_bypass(iwah, false);
						rt_printf("WAH Effect ENABLED\n");
					}
					else
					{
						iwah_bypass(iwah, true);
						rt_printf("WAH Effect BYPASSED\n");
					}
					
			}
			break;
			
		case SW_CTXT_REVERB :
			set_efx_reverb();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(gReverb_wet > 0.0) {
						gReverb_wet = 0.0;
						rt_printf("REVERB Effect BYPASSED\n");
					}
					else
					{
						gReverb_wet = gReverb_mix;
						rt_printf("REVERB Effect ENABLED\n");
					}
					
			}
			
			break;
		case SW_CTXT_PHASE :
			set_efx_phase();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(phaser_toggle_bypass(phaser)) 
					{
						rt_printf("PHASER Effect BYPASSED\n");
					}
					else
					{
						rt_printf("PHASER Effect ENABLED\n");
					}
					
			}
			
			break;
		case SW_CTXT_TREM :
			set_efx_trem();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(trem_toggle_bypass(trem)) 
					{
						rt_printf("TREMOLO Effect BYPASSED\n");
					}
					else
					{
						rt_printf("TREMOLO Effect ENABLED\n");
					}
					
			}
			
			break;	
		case SW_CTXT_ODRIVE :
			set_efx_overdrive();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(od_set_bypass(od, false)) // toggle
					{
						rt_printf("OVERDRIVE Effect BYPASSED\n");
					}
					else
					{
						rt_printf("OVERDRIVE Effect ENABLED\n");
					}
					
			}
			
			break;	
		case SW_CTXT_KLGN :
			set_efx_klingon();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(kot_set_bypass(ko, false)) // toggle
					{
						rt_printf("KLINGON Effect BYPASSED\n");
					}
					else
					{
						rt_printf("KLINGON Effect ENABLED\n");
					}
					
			}
			
			break;		
		case SW_CTXT_SUSTAIN :	
			set_efx_sustainer();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(fx_sustain.setBypass()) 
					{
						rt_printf("SUSTAIN Effect BYPASSED\n");
					}
					else
					{
						rt_printf("SUSTAIN Effect ENABLED\n");
					}
					
			}
			break;
		case SW_CTXT_GEQ1 :
			set_efx_geq1();
			break;
		case SW_CTXT_GEQ2 :
			set_efx_geq2();
			break;
		case SW_CTXT_ENVF :
			set_efx_envf();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(envf_toggle_bypass(ef)) 
					{
						rt_printf("ENVELOPE FILTER Effect BYPASSED\n");
					}
					else
					{
						rt_printf("ENVELOPE FILTER Effect ENABLED\n");
					}
					
			}
			break;
		case SW_CTXT_FBCOMP :
			set_efx_fbcomp();
			if(pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge == true)
			{
					pushbuttons[EFFECT_BYPASS_SWITCH].rising_edge = false;
					if(feedback_compressor_set_bypass(fbcompressor, false)) 
					{
						rt_printf("FEEDBACK COMPRESSOR Effect BYPASSED\n");
					}
					else
					{
						rt_printf("FEEDBACK COMPRESSOR Effect ENABLED\n");
					}
					
			}
			break;
			
		default :
			break;
	}
	if(pushbuttons[WAH_PEDAL_BYPASS].rising_edge)
	{
		pushbuttons[WAH_PEDAL_BYPASS].rising_edge = false;
		iwah_bypass(iwah, false);
		rt_printf("WAH Effect ENABLED\n");
	}
	else if (pushbuttons[WAH_PEDAL_BYPASS].falling_edge)
	{
		pushbuttons[WAH_PEDAL_BYPASS].falling_edge = false;
		iwah_bypass(iwah, true);
		rt_printf("WAH Effect DISABLED\n");
	}
	


}

void scan_inputs(BelaContext* context)
{

	float tmp = 0.0;
	
	if(knobs[gRoundRobin].knob_toggle_timer > 0) {
		knobs[gRoundRobin].knob_toggle_timer--;
		if(knobs[gRoundRobin].knob_toggle_timer == 0) rt_printf("KNOB toggle timer CH %d EXPIRED\n", gRoundRobin);
	}

	for(unsigned int n = 0; n < context->audioFrames; n++) 
	{
		if(!(n % gAudioFramesPerAnalogFrame)) {
				
			tmp = analogRead(context, n/gAudioFramesPerAnalogFrame, gRoundRobin);
			knobs[gRoundRobin].current_reading = tmp;

		}
		//Filter the analog channel readings
		knobs[gRoundRobin].filtered_reading[0] = knobs[gRoundRobin].a*knobs[gRoundRobin].current_reading + knobs[gRoundRobin].b*knobs[gRoundRobin].filtered_reading[0];		
		knobs[gRoundRobin].filtered_reading[1] = knobs[gRoundRobin].c*knobs[gRoundRobin].filtered_reading[0] + knobs[gRoundRobin].d*knobs[gRoundRobin].filtered_reading[1];	
		knobs[gRoundRobin].filtered_buf[n] = knobs[gRoundRobin].filtered_reading[1];
		rtprintclock++;

	}  //for()
	
	//evaluate switch switch_state (schmitt trigger on analog input channel)
	// cranking the knob from low to high will set the switch state high and reset hysteresis 
	// then cranking knob from high to low will set the state low and set threshold high again.
	if( (knobs[gRoundRobin].filtered_reading[1] >= SCHMITT_HIGH) && (knobs[gRoundRobin].knob_toggle_timer > 0) ) 
	{
		knobs[gRoundRobin].knob_toggle_timer = 0;
		if(knobs[gRoundRobin].switch_state == false) {
			knobs[gRoundRobin].switch_state = true;
			knobs[gRoundRobin].edge = true;
			//rt_printf("Analog in %d TOGGLED HIGH\n", gRoundRobin);
		} else {
			knobs[gRoundRobin].switch_state = false;
			knobs[gRoundRobin].edge = true;
			//rt_printf("Analog in %d TOGGLED LOW\n", gRoundRobin);
		}
		
	} 
	else if (knobs[gRoundRobin].filtered_reading[1] <= SCHMITT_LOW)
	{
		knobs[gRoundRobin].knob_toggle_timer = gKnobToggleTimeout;
	}
	
	//Check whether a control state changed in order to activate settings
	if(knobs[gRoundRobin].active_timer > 0) {
		knobs[gRoundRobin].active_timer--;
		
		// if(knobs[gRoundRobin].active_timer == 0){
		// 	rt_printf("ACTIVE TIMER RESET\n");
		// }
	}
	if( (knobs[gRoundRobin].scan_timer++) > knobs[gRoundRobin].scan_cycle) {
		knobs[gRoundRobin].scan_timer = 0;
		
		if( fabs(knobs[gRoundRobin].last_reading - knobs[gRoundRobin].filtered_reading[1]) > knobs[gRoundRobin].dVdt) {
			knobs[gRoundRobin].active_timer = knobs[gRoundRobin].active_reset;
			knobs[gRoundRobin].last_reading = knobs[gRoundRobin].filtered_reading[1];
			
			
			//rt_printf("CHANGE DETECTED\n");
			
		}
		
	}
	
	//Reset virtual multiplexer
	if(++gRoundRobin >= ANALOG_CHANNELS) {
		gRoundRobin = 0;	
	}

	
}

// Extract a single channel from BelaContext and format for flanger code input
void format_audio_buffer(BelaContext* context, float *outbuffer, int channel)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) 
	{
		// Read the audio input and half the amplitude
		outbuffer[n] = audioRead(context, n, channel);
		
	}
	
	//
	// HPF for DC removal.  Uncomment if DC removal patch to core is not applied.
	//
	
	// float x0 = 0.0;
	// float y0 = 0.0;
	// if(channel == 0)
	// {
	// 	for(unsigned int n = 0; n < context->audioFrames; n++) 
	// 	{
	// 		x0 = outbuffer[n];
	// 		y0 = ch0_dcr->y1 + x0 - ch0_dcr->x1;
	// 		y0 *= ch0_dcr->a;
	// 		ch0_dcr->x1 = outbuffer[n];
	// 		ch0_dcr->y1 = y0;
	// 		outbuffer[n] = y0;
			
	// 	}
		
	// } 
	// else if(channel == 1)
	// {
	// 	for(unsigned int n = 0; n < context->audioFrames; n++) 
	// 	{
	// 		x0 = outbuffer[n];
	// 		y0 = ch1_dcr->y1 + x0 - ch1_dcr->x1;
	// 		y0 *= ch1_dcr->a;
	// 		ch1_dcr->x1 = outbuffer[n];
	// 		ch1_dcr->y1 = y0;
	// 		outbuffer[n] = y0;
			
	// 	}	
	// }
}

void process_digital_inputs()
{
	// Effect parameters context selection
	if(pushbuttons[CONTROL_CONTEXT_SWITCH].rising_edge == true) {
		 if( ++fx_cntl[ain_control_effect].ain_control_context >= fx_cntl[ain_control_effect].ain_control_context_max ) 
		 {
		 		fx_cntl[ain_control_effect].ain_control_context = 0;
		 }
		 pushbuttons[CONTROL_CONTEXT_SWITCH].rising_edge = false;
		 
		 if(fx_cntl[ain_control_effect].ain_control_context == SW_CTXT_ENV)
		 {
		 	rt_printf("ACTIVATED SW_CTXT_ENV for effect %d (Control Page 2)\n", ain_control_effect);
		 } 
		 else if (fx_cntl[ain_control_effect].ain_control_context == SW_CTXT_NORM)
		 {
		 	rt_printf("ACTIVATED SW_CTXT_NORM for effect %d (Control Page 1)\n", ain_control_effect);
		 }
		 else if (fx_cntl[ain_control_effect].ain_control_context == SW_CTXT_PG3)
		 {
		 	rt_printf("ACTIVATED SW_CTXT_PG3 for effect %d (Control Page 3)\n", ain_control_effect);
		 }
		 else if (fx_cntl[ain_control_effect].ain_control_context == SW_CTXT_PG4)
		 {
		 	rt_printf("ACTIVATED SW_CTXT_PG4 for effect %d (Control Page 4)\n", ain_control_effect);
		 }
	}
	
	// Which effect is active 
	if(pushbuttons[CONTROL_EFFECT_SWITCH].rising_edge == true)
	{
		pushbuttons[CONTROL_EFFECT_SWITCH].rising_edge = false;
		if(++ain_control_effect >= NFX){
			ain_control_effect = 0;
		}

		if(ain_control_effect == SW_CTXT_FLANGE)
			rt_printf("Active Effect: FLANGE, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_DELAY)
			rt_printf("Active Effect: DELAY, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_CHORUS)
			rt_printf("Active Effect: CHORUS, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_WAH)
			rt_printf("Active Effect: WAH, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_REVERB)
			rt_printf("Active Effect: REVERB, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_PHASE)
			rt_printf("Active Effect: PHASER, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_TREM)
			rt_printf("Active Effect: TREMOLO, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_SUSTAIN)
			rt_printf("Active Effect: SUSTAIN, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_GEQ1)
			rt_printf("Active Effect: GEQ-PRE, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_GEQ2)
			rt_printf("Active Effect: GEQ-POST, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_ENVF)
			rt_printf("Active Effect: ENV FILTER, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_FBCOMP)
			rt_printf("Active Effect: FB COMPRESSOR, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_ODRIVE)
			rt_printf("Active Effect: OVERDRIVE, %d\n", ain_control_effect);
		else if (ain_control_effect == SW_CTXT_KLGN)
			rt_printf("Active Effect: KLINGON TONE, %d\n", ain_control_effect);
	}
	

}

void debounce_digital_inputs(BelaContext *context)
{
	int status;
	for(int i = 0; i < N_PBTNS; i++) 
	{
		status=digitalRead(context, 0, i); 
		if(status) {
			if(pushbuttons[i].integrator < pushbuttons[i].trigger) pushbuttons[i].integrator += 1;
		} else {
			if(pushbuttons[i].integrator > 0) pushbuttons[i].integrator -= 1;
		}
		
		if(pushbuttons[i].integrator == pushbuttons[i].trigger) {
			//Edge detection and toggle
			if(pushbuttons[i].state == false){
				//Getting here means it was a rising edge
				pushbuttons[i].rising_edge = true; 

				//rt_printf("RISING EDGE Detected: %d\n", i);
				
				//Then use rising edge to toggle the state
				pushbuttons[i].toggle = !(pushbuttons[i].toggle);
				if(pushbuttons[i].toggle == true) {
					rt_printf("Toggle State SET: %d\n", i);

				}
				if(pushbuttons[i].toggle == false) {
					rt_printf("Toggle State RESET: %d\n", i);
				}
			}
			pushbuttons[i].state = true;
		}
		if(pushbuttons[i].integrator == 0) {
			//If current state was true and we're changing to false then it's a falling edge 
			if(pushbuttons[i].state == true){
				pushbuttons[i].falling_edge = true;
				//rt_printf("FALLING EDGE Detected: %d\n", i);	
			}
			//Then assign the new state 
			pushbuttons[i].state = false;
		}
		
		
		
		//read the value of the button
	}

}

void setup_digital_inputs(BelaContext *context)
{
	for(int i = 0; i < N_PBTNS; i++) 
	{
		pinMode(context, 0, i, INPUT);
		pushbuttons[i].integrator = 0;
		pushbuttons[i].trigger = 10;
		pushbuttons[i].state = false;
		pushbuttons[i].toggle = false;
		pushbuttons[i].rising_edge = false;
		pushbuttons[i].falling_edge = false;
	}

	for(int i = 0; i < NFX; i++) 
	{	
		fx_cntl[i].ain_control_context = SW_CTXT_NORM;
		fx_cntl[i].ain_control_context_max = 2;
	}
	
	//Envelope filter has more than default control pages
	fx_cntl[SW_CTXT_ENVF].ain_control_context_max = 4;
	
	gSW_timer_max = T_SW_SCAN*context->digitalSampleRate/( context->digitalFrames * 1000);
	gSW_timer = 0;
	

}

bool setup(BelaContext *context, void *userData)
{
	gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
	gifs = 1.0/context->audioSampleRate;
	gNframes = context->audioFrames;
	
	delayline = tflanger_init(delayline, T_ECHO, context->audioSampleRate);
	chorus = tflanger_init(delayline, T_CHORUS, context->audioSampleRate);
	flanger = tflanger_init(delayline, T_FLANGE, context->audioSampleRate);

	ch0 = (float*) malloc(sizeof(float)*context->audioFrames);
	ch1 = (float*) malloc(sizeof(float)*context->audioFrames);
	
	ch0_dcr = (dc_remover*) malloc(sizeof(dc_remover));
	ch1_dcr = (dc_remover*) malloc(sizeof(dc_remover));
	
	ch0_dcr->fs = context->audioSampleRate;
	ch0_dcr->ifs = gifs;
	ch0_dcr->a = expf(-ch0_dcr->ifs*50.0); //approximately 8 Hz
	ch0_dcr->x1 = 0.0;
	ch0_dcr->y1 = 0.0;
	
	ch1_dcr->fs = context->audioSampleRate;
	ch1_dcr->ifs = gifs;
	ch1_dcr->a = expf(-ch0_dcr->ifs*50.0); //approximately 8 Hz
	ch1_dcr->x1 = 0.0;
	ch1_dcr->y1 = 0.0;
	

	float dpfc = 2.0*M_PI*CF_CUTOFF*(gifs*ANALOG_CHANNELS);
	for(unsigned int i = 0; i<ANALOG_CHANNELS; i++)
	{
		knobs[i].current_reading = 0.25;
		knobs[i].filtered_reading[0] = 0.5;
		knobs[i].filtered_reading[1] = 0.5;
		knobs[i].filtered_buf = (float*) malloc(sizeof(float)*context->audioFrames);
		knobs[i].a = dpfc/(dpfc + 1.0);
		knobs[i].b = 1.0 - knobs[i].a;
		knobs[i].c = 0.5*dpfc/(dpfc + 1.0);
		knobs[i].d = 1.0 - knobs[i].c;
		
		//Knob switch functions
		knobs[i].switch_state = true;
		knobs[i].edge = false;
		knobs[i].knob_toggle_timer = 0;

		//State change detection
		knobs[i].active_timer = 0;
		knobs[i].active_reset = (int)(2*context->audioSampleRate)/(ANALOG_CHANNELS * gNframes);
		knobs[i].scan_cycle = ((int) context->audioSampleRate) * SCAN_PERIOD / (1000 * ANALOG_CHANNELS * gNframes);
		knobs[i].scan_timer = 0;
		knobs[i].last_reading = 0.5;
		knobs[i].dVdt = 0.01;
		
	}
	//initialize LFO shapes for each effect 
	gDLY_lfo_type = SINE;
	gCHOR_lfo_type = INT_TRI;
	gFLANGE_lfo_type = EXP;

	//Analog input control context
	gKnobToggleTimeout = (KNOB_SWITCH_TIMEOUT*((unsigned int) context->audioSampleRate))/(gNframes * ANALOG_CHANNELS); 
	ain_control_effect = SW_CTXT_DELAY;
	startup_mask_timer = 4*((int) context->audioSampleRate);
	
	setup_digital_inputs(context);

	tflanger_setPreset(chorus, 0);
	tflanger_setPreset(delayline, 1);
	tflanger_setPreset(flanger, 2);
	
	tflanger_setFinalGain(delayline, 0.0);
	tflanger_setFinalGain(chorus, 0.0);
	tflanger_setFinalGain(flanger, 0.0);
	
	//Setup wah wah 
	iwah = make_iwah(iwah, context->audioSampleRate);
	iwah_circuit_preset(VOX, iwah, context->audioSampleRate );
	iwah_bypass(iwah, true);
	
	//Zita1 reverb
	zita1.init(context->audioSampleRate, false, context->audioFrames); //no ambisonic processing 
	gReverb_wet = 0.5f;
	gReverb_mix = gReverb_wet;
	
	//Phaser
	phaser = make_phaser(phaser, context->audioSampleRate);
	phaser_circuit_preset(PHASE_90, phaser );
	
	//Tremolo
	trem = make_trem(trem, context->audioSampleRate);
	
	//Sustainer
	fx_sustain.init(context->audioSampleRate, gNframes);
	fx_sustain.setpreset (0);
	
	//Compressor
	fbcompressor = make_feedback_compressor(fbcompressor, context->audioSampleRate, gNframes);
	
	//Overdrive 
	od = make_overdrive(od, 1, context->audioFrames, context->audioSampleRate);
    od_set_bypass(od, true);

	//Klingon-Tone 
	ko = make_klingon(ko, 1, context->audioFrames, context->audioSampleRate);
    kot_set_bypass(ko, true);
	
	// Graphic EQ 1
	geq1 = make_equalizer(geq1, 6, 164.0, 5980.0, context->audioSampleRate);
	
	// Graphic EQ 2
	geq2 = make_equalizer(geq2, 6, 164.0, 5980.0, context->audioSampleRate);
	
	//Envelope filter
	ef = envf_make_filter(ef, context->audioSampleRate, gNframes);
	gMaster_Envelope = (float*) malloc(sizeof(float)*gNframes);
	for(int i = 0; i<gNframes; i++)
		gMaster_Envelope[i] = 0.0;
		
	//
	// DEBUG (scope)
	//
	
	scope.setup(2, context->audioSampleRate);
	
	return true;
}


void render(BelaContext *context, void *userData)
{
	format_audio_buffer(context, ch1, 1);
	format_audio_buffer(context, ch0, 0);
	scan_inputs(context);
	debounce_digital_inputs(context);
	process_digital_inputs();
	apply_settings();

	//CH0 effects
	feedback_compressor_tick_n(fbcompressor, ch0, gMaster_Envelope);
	geq_tick_n(geq1, ch0, gNframes);
	fx_sustain.tick_n(ch0);
	envf_tick_n(ef, ch1, gMaster_Envelope);
	iwah_tick_n(iwah, ch0, knobs[EXP_PEDAL].filtered_buf, gNframes);
	overdrive_tick(od, ch0);
	klingon_tick(ko, ch0);
	trem_tick_n(trem, ch0, gNframes);

	//CH1 effects
	tflanger_tick(delayline, gNframes, ch1, gMaster_Envelope);
	tflanger_tick(chorus, gNframes, ch1, gMaster_Envelope);
	tflanger_tick(flanger, gNframes, ch1, gMaster_Envelope);
	phaser_tick_n(phaser, gNframes, ch1);
	zita1.tick_mono(gNframes, ch1);
	
	
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		if(startup_mask_timer > 0) 
			startup_mask_timer--;
		//scope.log(gMaster_Envelope[n], ch0[n]);
		//scope.log(ch0[n], ch1[n]);
		audioWrite(context, n, 0, ch0[n]);
		audioWrite(context, n, 1, ch1[n]);
	}	
}

void cleanup(BelaContext *context, void *userData)
{

}
