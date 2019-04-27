//
// KOT style tonestack
//

#ifndef KOT_STACK_H
#define KOT_STACK_H

#include "iir_1pole.h"

typedef struct kot_stack_t
{
    float fs;  //Samplerate

    // Circuit component values
    float rtone;  // Tone pot resistance value
    float rboost; // Boost pot resistance value
    float ri;     // Input feed resistor
    float rc;     // Resistor joining tone pot wiper to boost pot
    float ro;     // Total output load resistance
    float c1;     // Tone capacitor
    float c2;     // Boost capacitor

    // user-configurable params
    float tone_pot_pos;
    float boost_pot_pos;

    // Coefficients
    iir_1p st1;
    iir_1p st2;

} kot_stack;

void kotstack_init(kot_stack* ks, float fs_);

// User functions
void kotstack_set_tone(kot_stack* ks, float tone);
void kotstack_set_boost(kot_stack* ks, float boost);

// Run filter
inline float kotstack_tick(kot_stack* ks, float x)
{
    return tick_filter_1p_g(&(ks->st2), tick_filter_biquad(&(ks->st1), x) );
}

#endif //KOT_STACK_H
