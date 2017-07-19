
#ifndef IWAH_H
#define IWAH_H

#define GCB 	0	//Common Dunlop GCB-95
#define VOX 	1 	//Vox V847 voicing
#define DUNLOP	2	//"The Original" Crybaby
#define MCCOY	3 	//Clyde McCoy voicing
#define VOCAL	4   //Vox with a little extra Q and some low-end brought up a bit
#define EXTREME 5 	//crazy synth-like
#define MAX_WAHS 6  //Update as wahs are added

typedef struct iwah_t {
    //Circuit parameters
    //Using these makes it straight-forward to model other 
    //variants of the circuit
    float Lp; //RLC tank inductor
    float Cf; //feedback capacitor
    float Ci; //input capacitor
    float re; //equivalent resistance looking into input BJT base
    float Rp; //resistor placed parallel with the inductor
    float Ri; //input feed resistor
    float Rs; //RLC tank to BJT base resistor (dry mix)
    
    //Gain-setting components
    float Rc; //BJT gain stage collector resistor
    float Rpot; //Pot resistance value
    float Rbias; //Typically 470k bias resistor shows up in parallel with output
    float Re; //BJT gain stage emitter resistor
    float gf; //forward gain of BJT amplifier

    //High-Pass biquad coefficients
    float b0h, b1h, b2h;
    float a0h, a1h, a2h;

    //Band-Pass biquad coefficients
    float b0b, b2b;
    float a0b, a1b, a2b;

    //Final combined biquad coefficients used by run_filter()
    float b0;
    float b1;
    float b2;
    float a0, a0c;
    float a1, a1c;
    float a2, a2c;
    float ax; //this is used to store 1/a0 to avoid several 1/x operations

    //First order high-pass filter coefficients
    //y[n] = ghpf * ( x[n] - x[n-1] ) - a1p*y[n-1]
    float a1p;
    float ghpf;

    //biquad state variables
    float y1;
    float y2;
    float x1;
    float x2;

    //First order high-pass filter state variables
    float yh1;
    float xh1;
    
    //Bypass the effect
    bool bypass;

} iwah_coeffs;

//Initialize the struct and allocate memory
iwah_coeffs* make_iwah(iwah_coeffs*, float);

//(float x, float gp, iwah_coeffs* cf)
// input sample, pot gain (0...1), iwah struct
float iwah_tick(float, float, iwah_coeffs*);
void iwah_tick_n(iwah_coeffs*, float*, float*, int );

//Select preset circuit voicings
void iwah_circuit_preset(int , iwah_coeffs*, float );

//Bypass effect
void iwah_bypass(iwah_coeffs*, bool);

#endif //IWAH_H

