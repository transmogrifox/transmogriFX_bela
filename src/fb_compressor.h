#ifndef FB_COMPRESSOR_H
#define FB_COMPRESSOR_H

typedef struct feedback_compressor_t
{
    //System Parameters
    float fs;  //Sample rate
    float ifs; //Inverse sample rate
    int N;     //Processing block size

    //User settings
    bool bypass, reset;
    bool linmode;
    float threshold_db;
    float ratio;
    float t_attack;
    float t_release;
    float out_gain;

    //Internal variables
    float atk, atk0, rls;  //attack and release coefficients
    float lmt_atk;  //Attack time when limiting
    float lmt_thrsh;  //Sets when to start speeding up attack time
    float k, tk, tkp1, mk, tpik, ak;  //Related to compression ratio
    float t;  //Threshold
    float g;  //Static gain (makeup_gain*out_gain)
    float db_dynrange;   //how much gain reduction will be allowed
    float dynrange;
    float makeup_gain;
    float wet, dry;

    //Peak detector
    float pkrls;
    float pk_hold_time;
    
    //knee
    bool soft_knee;
    float knt, hknt, iknt;

    //state variables
    float gain;
    float y1;
    float pk;
    float rr[4];
    int rr_cyc;
    int pk_timer;

} feedback_compressor;

//Allocate and initialize struct with sane values
//fs = Sampling Frequency
//N = processing block size
feedback_compressor*
make_feedback_compressor(feedback_compressor* fbc, float fs, int N);

//This is where it happens
//x[] is expected to be of length fbc->N
void
feedback_compressor_tick_n(feedback_compressor* fbc, float *x, float *envelope);

//Settings

//Compression onset threshold
//Units of dB: -infinity to 0 dB
void
feedback_compressor_set_threshold(feedback_compressor* fbc, float t_);

//Compression ratio
//This is not a ratio in the traditional sense if linmode is set 'true'.
//In this mode the amount of perceived compression roughly correlates to typical feed-forward
//compression ratio. If input level is 0 dB, then output is
//-threshold + threshold/ratio, but in between is not log-linear
//This mode is most useful with lower threshold and lower ratio settings
//as it has the effect of a continuously increasing ratio with signal level--
//threshold behaves like a knee radius making for a very soft-knee compression.
//
//When linmode == 'false' then the exponential feedback gain is applied and 
//the ratio represents the traditional log-linear transfer function
//
//input unitless: 1 to 20
void
feedback_compressor_set_ratio(feedback_compressor* fbc, float r_);

//input units in ms:  1 ms to 1000 ms
void
feedback_compressor_set_attack(feedback_compressor* fbc, float a_);

//input units in ms:  10 ms to 1000 ms
void
feedback_compressor_set_release(feedback_compressor* fbc, float r_);

//input g in units of dB: -db_dynrange (-60dB) to 20 dB
void
feedback_compressor_set_out_gain(feedback_compressor* fbc, float g_db);

//Parallel compression option (wet/dry mix).  Wet range: 0.0 to 1.0
//Can help preserve some of the transients while providing sustain
void
feedback_compressor_set_mix(feedback_compressor* fbc, float wet);

//Either soft (true) or hard (false)
//Default is true (soft knee)
void
feedback_compressor_set_knee(feedback_compressor* fbc, bool sk);

//Either linear feedback mode (true) or 
//inverse exponential feedback yielding a traditional log-linear transfer function (false)
//Default is false (traditional compression curve)
void
feedback_compressor_set_transfer_function(feedback_compressor* fbc, bool tf);

//if bp = true, it is forced to bypass mode and function returns true.
//if bp = false, bypass state is toggled and new state is returned
bool
feedback_compressor_set_bypass(feedback_compressor* fbc, bool bp);


//More for internal use
void
feedback_compressor_update_parameters(feedback_compressor* fbc);

//deallocate memory when finished using the struct
void
feedback_compressor_destructor(feedback_compressor* fbc);

#endif //FB_COMPRESSOR_H
