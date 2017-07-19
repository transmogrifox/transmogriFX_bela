#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "fb_compressor.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <math_neon.h>

#include "fb_compressor.h"

//
//   Parallel Control                 Feedback Sidechain Works on Envelope
//  ^^^^^^^^^^^^^^^^^                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//   xn--O--->[Peak Detector]-------->{X}----------------------O--->yn (envelope)
//       |                             |                       |
//      {X}<----Control Gain------O----+                [ATTACK/RELEASE]<------+
//       |                        |                     [ (Ballistics) ]       |
//      {X}<--Makeup Gain    |->[OR]<-{+}<--1                  |               |
//       |                   {e^-KX}   |-                      |               |
//      {X}<--Output Level   [!lin?]<-{X}<--K (ratio)         {+}_<--t         |
//       |                             |                       |               |
//       |                             |_______________________|               |
//       |                                                                     |
//       O---->[Output over range detection}-----------------------------------+
//       |
//       +--[CLIP]-->yo {Final Output}
//
//  Not shown:  Knee Processing, Wet/Dry Mix
//


feedback_compressor*
make_feedback_compressor(feedback_compressor* fbc, float fs, int N)
{
    fbc = (feedback_compressor*) malloc(sizeof(feedback_compressor));

    fbc->fs = fs;
    fbc->ifs = 1.0/fs;
    fbc->N = N;

	fbc->linmode = false; //continuously increasing ratio (true) or log-linear (false)?
    fbc->threshold_db = -36.0;
    fbc->ratio = 8.0;
    fbc->db_dynrange = 60.0;
    feedback_compressor_set_out_gain(fbc, -6.0);
    fbc->t_attack = 50.0e-3;
    fbc->t_release = 75.0e-3;
    fbc->wet = 1.0;
    fbc->dry = 0.0;

    fbc->atk = expf(-fbc->ifs/(fbc->t_attack));
    fbc->atk0 = fbc->atk;
    fbc->lmt_atk = expf(-fbc->ifs/(0.001));
    fbc->rls = expf(-fbc->ifs/(fbc->t_release));
    fbc->pkrls = expf(-fbc->ifs/(0.008));
    fbc->pk_hold_time = lrintf(0.0042*fbc->fs);

    feedback_compressor_update_parameters(fbc);
    fbc->dynrange = powf(10.0, -fbc->db_dynrange/20.0);

    fbc->gain = 1.0;
    fbc->y1 = 0.0;
    fbc->pk = 0.0;
    fbc->pk_timer = 0;
    fbc->rr_cyc = 0;
    for(int n = 0; n < 4; n++)
    {
   		fbc->rr[n] = 0.0;
    }

    fbc->bypass = true;
    fbc->reset = true;  //everything has been initialized
    fbc->soft_knee = true;

    return fbc;
}

void
feedback_compressor_tick_n(feedback_compressor* fbc, float *x, float *envelope)
{
	// if(fbc->bypass)
 //   {
 //       if(!fbc->reset)
 //       {
 //           fbc->gain = 1.0;
 //           fbc->y1 = 0.0;
 //           fbc->pk = 0.0;
 //           fbc->pk_timer = 0;
 //           fbc->atk0 = fbc->atk;
 //           fbc->reset = true;
 //       }
 //       return;
 //   }
 	bool use_limit = true;

    float yn;
    for(int i = 0; i < fbc->N; i++)
    {
        //Run Sidechain

        //Peak detector
        //  This is a "round-robin" style peak detector.
        //  General strategy inspired by Harry Bissel:
        //   http://www.edn.com/design/analog/4344656/Envelope-follower-combines-fast-response-low-ripple
        //   http://m.eet.com/media/1143121/18263-figure_1.pdf
        //  A circular buffer (fbc-rr[]) is used to store
        //  peak values and it is clocked every pk_hold_time.
        //  The oldest peak value is reset and the maximum
        //  of the circular buffer is used to represent the
        //  envelope.
        //  The output is a square-stepped track & hold
        //  signal that closely follows the signal envelope.
        //  Finally the rapid edges must be smoothed, so
        //  a last stage is added to filter the peak detector.
        //  The end result is a very smooth approximation of the
        //  envelope.
        //  The key is matching the round-robin cycling rate
        //  to the expected type of signal and desired response times.
        //  For guitar and bass it should be set to make a full cycle at about
        //  50 Hz, and hold time is (1/2)*(1/4) of that period.
        //  Bass low E is around 38 Hz, so may want to go slower for bass
        //  If response ends up being too fast, but ballistics filtering
        //  is expected to make up the difference on that front
        yn = fabs(x[i]);
    	if(yn > fbc->rr[fbc->rr_cyc])
    	{
    		fbc->rr[fbc->rr_cyc] = yn;
    	}

        //Cycle Timer
        if (fbc->pk_timer < fbc->pk_hold_time)
        {
            fbc->pk_timer++;
        }
        else
        {
            if(++fbc->rr_cyc >= 4)
        		fbc->rr_cyc = 0;
        	fbc->pk_timer = 0;
        	fbc->rr[fbc->rr_cyc] = 0.0;
        }

        //Final peak value selection circuit
        float tmpk = 0.0;
        for(int n = 0; n < 4; n++)
        {
        	if(fbc->rr[n] > tmpk)
        		tmpk = fbc->rr[n];
        }
        //Smooth the stair-steps
        fbc->pk = tmpk + (fbc->pk - tmpk)*fbc->pkrls;
        envelope[i] = fbc->pk;
        //
        // End Peak Detector
        //

        //last computed gain on new peak detector value is used
        //to compare against the threshold
        yn = fbc->gain*fbc->pk;
        if(fbc->linmode)
        {
	        if(yn < fbc->t)
	        {
	        	yn = fbc->t;
	        }
	        //Apply limit to error signal.
	        // Otherwise the excessive overdrive pushes attack time to almost nil
	        else if (yn > fbc->tpik)
	        {
	            yn = fbc->tpik;
	        }
        }
        else
        {
	        if(yn < fbc->t)
	        {
	        	yn = fbc->t;
	        }
	        //Apply limit to error signal.
	        // Otherwise the excessive overdrive pushes attack time to almost nil
	        else if (yn > 1.0)
	        {
	            yn = 1.0;
	        }
        }

		//
		//Ballistics
		//
        //Attack/Release on sidechain envelope
        if(yn > fbc->y1) //Attack
            fbc->y1 = yn + (fbc->y1 - yn)*fbc->atk0;
        else //Release
            fbc->y1 = yn + (fbc->y1 - yn)*fbc->rls;
        //
        // End Ballistics
        //

        //Soft knee implementation
        float sk = fbc->y1 - fbc->t;
        float yk = 0.0;
        float gk = 0.0;
        if(fbc->soft_knee)
        {
        	if(sk < fbc->hknt)
        	{
        		gk = 1.0 - (fbc->knt - sk) * fbc->iknt;
        	}
        	else
        	{
        		gk = 1.0;
        		yk = -0.25*fbc->knt;
        	}
        	yk += sk*gk;
        	yk += fbc->t;

        }
        else
        {
        	yk = fbc->y1;
        }
        //End soft knee

        //Compute gain reduction
        // This relationship is refactored to a more computationally
        // efficient form from a block diagram of the feedback
        // compressor ( gain = 1 - k*(y - t) ).
        // or to do log-linear curve,
        // gain = e^(k*(t-y))

        if(fbc->linmode)
        {
        	//fbc->gain = 1.0 + fbc->y1*fbc->mk + fbc->tk; < further refactored
        	fbc->gain = yk*fbc->mk + fbc->tkp1;
        }
    	else //linear in log domain
    	{
    		fbc->gain = expf_neon( fbc->ak*(fbc->tk + yk*fbc->mk) );
            //fbc->gain = expf( fbc->ak*(fbc->tk + yk*fbc->mk) );
    	}

        if(fbc->gain < fbc->dynrange)
            fbc->gain = fbc->dynrange;
        else if (fbc->gain > 1.0) //paranoia
            fbc->gain = 1.0;
        //
        // End gain computation
        //

        //
        //Apply gain (this is the final output)
        // Processing x[i] outside of the feedback loop
        // is a bit of a book-keeping trick.  It helped prevent
        // accidential bug injection.
        // The feedback loop works around the peak detector,
        // which is directly proportional to the input x[i].
        // The final output behavior is equivalent to an analog circuit
        // implementation of a feedback compressor.
        if(!fbc->bypass)
        {
        	x[i] = fbc->wet*fbc->g*fbc->gain*x[i] + fbc->dry*x[i];
        	use_limit = true;
        } else 
        	use_limit = false;
        //x[i] *= fbc->gain;

        //
        // End gain processing
        //

        //
        // Limiting
        //
        
        // increase attack time for fast limiting if over 1.0
        if(use_limit)
        {
            float tmp = fabs(x[i]) - fbc->lmt_thrsh;
            //If over 1.0, limit very rapidly
            if(tmp > 1.0)
            {
                fbc->atk0 = fbc->lmt_atk;
            }
            //Gradually decrease attack time when X dB (represented by lmt_thrsh)
            //above out_gain
            else if(tmp > 0.0 )
            {
                fbc->atk0 = tmp*fbc->lmt_atk + (1.0 - tmp)*fbc->atk;
            }
            //Otherwise apply normal attack time
            else
            {
                fbc->atk0 = fbc->atk;
            }
            //Hard clip what remains outside of limit.
            if(x[i] > 1.0)
                x[i] = 1.0;
            if(x[i] < -1.0)
                x[i] = -1.0;
        }
        //
        // End Limiting
        //
    }

}

void
feedback_compressor_update_parameters(feedback_compressor* fbc)
{

	if(fbc->soft_knee)
	{
		fbc->t =  powf(10.0, (fbc->threshold_db - 3.0)/20.0);
	    //knee
	    fbc->knt = fbc->t;
	    fbc->hknt = 0.5*fbc->knt;
	    fbc->iknt = 1.0/fbc->knt;
	}
	else
	{
		fbc->t =  powf(10.0, fbc->threshold_db/20.0);
	}
    //Target makeup gain needed for given threshold and ratio setting
    float m = powf(10.0, (fbc->threshold_db/fbc->ratio - fbc->threshold_db)/20.0);
    fbc->makeup_gain = m;
    //Feedback ratio dependent upon both threshold and ratio
    fbc->k = (m - 1.0)/(1.0 - m*fbc->t);
    fbc->mk = -fbc->k;
    fbc->tk = fbc->t*fbc->k;
    fbc->tkp1 = fbc->tk + 1.0;
    fbc->tpik = fbc->t+1.414/fbc->k;
    float y = powf(10.0, fbc->threshold_db*(1.0 - 1.0/fbc->ratio)/20.0);
    fbc->ak = logf(1.0 + fbc->tk - y * fbc->k)/(fbc->tk - y * fbc->k);
    if(fbc->soft_knee)
        fbc->g = fbc->out_gain*m*0.85;  //makeup gain - 1.5 dB
    else
        fbc->g = fbc->out_gain*m;  //makeup gain

}

void
feedback_compressor_set_threshold(feedback_compressor* fbc, float t_)
{
    float t = t_;
    if(t > 0.0) t = 0.0;
    fbc->threshold_db = t;

    feedback_compressor_update_parameters(fbc);
}

void
feedback_compressor_set_ratio(feedback_compressor* fbc, float r_)
{
    float r = r_;
    if(r < 1.0) r = 1.0;
    else if(r > 20.0) r = 20.0;
    fbc->ratio = r;

    feedback_compressor_update_parameters(fbc);
}

void
feedback_compressor_set_attack(feedback_compressor* fbc, float a_)
{
    //Expects units are in ms
    float a = a_;

    if(a < 0.1) a = 0.1;  //needs to be some rate limiting for control loop stability
    else if ( a > 1000.0 ) a = 1000.0; //more than 1s attack is probably not useful and likely a bug.

    a *= 0.001;  //convert to units of seconds
    fbc->t_attack = a;
    fbc->atk = expf(-fbc->ifs/(fbc->t_attack));
    fbc->atk0 = fbc->atk;

}

void
feedback_compressor_set_release(feedback_compressor* fbc, float r_)
{
    //Expects units are in ms
    float r = r_;

    if(r < 10.0) r = 10.0;  //less than this is probably not useful
    else if ( r > 1000.0 ) r = 1000.0; //more is probably not useful and likely a bug.

    r *= 0.001;  //convert to units of seconds
    fbc->t_release = r;
    fbc->rls = expf(-fbc->ifs/(fbc->t_release));

}

//input g in units of dB
void
feedback_compressor_set_out_gain(feedback_compressor* fbc, float g_db)
{
    float g = g_db;
    if(g < -fbc->db_dynrange) g = -fbc->db_dynrange;
    else if (g > 20.0) g = 20.0;

    fbc->out_gain = powf(10.0, g/20.0);
    fbc->lmt_thrsh =  powf(10.0, (g + 6.0)/20.0);
    if(fbc->lmt_thrsh > 1.0) fbc->lmt_thrsh = 1.0;
    fbc->g = fbc->out_gain*fbc->makeup_gain;
}

void
feedback_compressor_set_mix(feedback_compressor* fbc, float wet)
{
	float w = wet;
	if(w > 1.0) w = 1.0;
	else if (w < 0.0) w = 0.0;

	fbc->wet = w;
	fbc->dry = 1.0 - w;
}

//Either soft (true) or hard (false)
void
feedback_compressor_set_knee(feedback_compressor* fbc, bool sk)
{
	fbc->soft_knee = sk;
	feedback_compressor_update_parameters(fbc);
}

void
feedback_compressor_set_transfer_function(feedback_compressor* fbc, bool tf)
{
	fbc->linmode = tf;
	feedback_compressor_update_parameters(fbc);
}

//if bp = true, it is forced to bypass mode.
//if bp = false, bypass state is toggled and new state is returned
bool
feedback_compressor_set_bypass(feedback_compressor* fbc, bool bp)
{
	if(!bp)
	{
		if(fbc->bypass)
			fbc->bypass = false;
		else
        {
            fbc->bypass = true;
            fbc->reset = false;
        }
        feedback_compressor_update_parameters(fbc);

	}
	else
	{
		fbc->bypass = true;
        fbc->reset = false;
	}

	return fbc->bypass;
}

void
feedback_compressor_destructor(feedback_compressor* fbc)
{
	free(fbc);
}
