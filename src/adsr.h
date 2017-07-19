/*
*   ADSR implementation
*/

#ifndef ADSR_H
#define ADSR_H

//ADSR States
#define ADSR_STATE_ATTACK   0
#define ADSR_STATE_DECAY    1
#define ADSR_STATE_SUSTAIN  2
#define ADSR_STATE_RELEASE  3

typedef struct adsr_t
{
    // System properties
    float fs, ifs;          //Sampling frequency and its inverse
    int NS;                 //Number of samples in loop

    // Parameters
    float amplitude;//Final peak value 
    float sustain;  //Expressed as aratio of amplitude: 0.0 to 1.0
    float velocity; //Expressed as aratio of amplitude: 1.0 to inf
    float pk;		//computed from velocity and amplitude
    float atk;      //A
    float dcy;      //D
    float sus;      //S
    float rls;      //R
    int trig_timeout;  //max time to hold trigger if not externally reset

    // State variables
    int state;      //ADSR State machine current state
    int trig_timer; //can set trigger and have it automatically clear
    float sv;       //the ADSR state variable
} adsr;

//Use this function to instantiate a new adsr struct
adsr*
make_adsr(adsr* ad, float fs, int N);

//This is where it all happens.
// It assumes "output" is pre-allocated to > size N
// given in make_adsr()
void
adsr_tick_n(adsr* ad, float* output);

//User parameters

//Output level (scale)
void
adsr_set_amplitude(adsr* ad, float a);

//Velocity -- default is sqrt(2)
void
adsr_set_velocity(adsr* ad, float v);

//Attack time in units of ms
void
adsr_set_attack(adsr* ad, float a);

//Decay time in units of ms
void
adsr_set_decay(adsr* ad, float d);

//Sustain gain 0 to 1
void
adsr_set_sustain(adsr* ad, float s);

//Release time in units of ms
void
adsr_set_release(adsr* ad, float r);

//
//  Triggering
//
//  The ADSR is triggered by making a function call to
//     adsr_set_trigger_state(ad, true).
//  When above is called, it "ticks off" the ADSR into ATTACK state.
//  It will then transition through ATTACK and RELEASE and hold on SUSTAIN
//  until the trigger is cleared.
//
//  Two conditions clear the trigger:
//     1)  Timer decrements to zero and causes ADSR to enter RELEASE state.
//         For timed operation set timeout in seconds with a function call
//         to adsr_set_trigger_timeout(ad, SECONDS).
//     2)  A call to adsr_set_trigger(ad, false) clears the trigger if not
//         already clear.  Clearing it when already clear has no effect on
//         the ADSR state machine.
//
//  Infinite sustain, or more practically behavior in which the trigger
//  can only be manually cleared is achieved by setting the trigger timeout
//  to a value less than zero, for example:
//     adsr_set_trigger_timeout(ad, -1.0)
//  In above case the timer never decrements and cannot clear trigger.
//  The consequence is the ADSR settles to "SUSTAIN" state indefinitely
//  The only way to release the ADSR is to clear it with
//  a call to adsr_set_trigger_state(ad, false), as illustrated
//  in point (2) above.
//

//Set trigger: "true", Clear trigger "false"
void
adsr_set_trigger_state(adsr* ad, bool t);

//Set trigger timeout if not manually resetting it
//takes time in seconds
//negative number means no timeout
void
adsr_set_trigger_timeout(adsr* ad, float t);

#endif //ADSR_H
