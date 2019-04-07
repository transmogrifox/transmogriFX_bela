# transmogriFX_bela
Audio Multi-Effects processor for Bela (http://bela.io)

Code is released to the public domain under the UNLICENSE
For more information, please refer to <http://unlicense.org/>

Contains code for a multi-effects processor for Bela.  Designed with default settings optimized for guitar use, but not limited strictly to guitar applications.

The contained code is not designed to compile and run outside of the Bela core system.  However, the individual effects files are mostly self-contained and do not rely on a web of dependencies, making it easy to fork the individual audio effects for inclusion in other projects. In case of code re-use elsewhere, the Bela-specific file Render.cpp can be studied for example usage.

Description
-------------
The high-level design strategy is to implement Bela as a mono processor instead of stereo.  Currently Channel 0 includes wah, compressor, sustainer and one of the Equalizers.  Channel 1 includes the modulation and delay effects for post processing.  

Output of Channel 0 then acts as an FX loop send and Channel 1 acts as an FX loop return.  This would be an ideal location to insert dirt boxes.

With current project configuration settings the latency is approximately 1.2ms per channel, so overall latency when using both channels in serial is 2.4ms, or equivalent to standing 2-1/2 ft away from your speaker/guitar amp.  

The latency is too short to be noticed, but too long to mix back in with a "dry" signal.  Mixing with a "dry" signal that was not pre-delayed would create a comb filter with the lowest frequency notch at roughly 800 Hz (it would be like a flanger stuck in a static position like some stompboxes label "filter matrix").

Current effects in project:
## Distortion
***Overdrive*** -- 4x linear over-sampled waveshaper with pre-emphasis, de-emphasis and 1rst-order shelving tone control.

***Klingon-Tone*** --  Freely roll your eyes.  This is a shameless knock-off of an overdrive pedal that sounds phonetically similar to "Klingon".  In its current state the EQ stages are correct to the original circuit except for the tone.  The tonestack is approximated.  A good sound can be had from this module but there is no claim being made that this is a faithful emulation of the original. 

## Dynamics

  ***Dynamic Range Compressor*** -- includes typical pro compressor controls:
      Threshold, Ratio, Attack, Release, Gain, Soft/Hard Knee, Mix(Parallel compression)

  ***Sustainer*** -- Simplified 2-knob compressor.  Is also more "open" sounding than the traditional compressor.

  ***Envelope Filter*** -- Envelope control of State Variable Filter (see below).

## Modulation

  ***Chorus*** -- includes envelope control options for almost every user-accessible paramter.

  ***Flanger*** -- includes envelope control options for almost every user-accessible paramter.

  ***Phaser***  -- default configuration is a 4-stage phaser with the MXR Phase 90 topology.

  ***Tremolo*** -- straightforward throbbing effect with several LFO shapes available.

## Filters

  ***Wah Wah*** -- Inductor wah DSP Model (based on classic Vox and Crybaby circuits)

  ***State Variable filter*** -- Implements Envelope and sequenced ADSR control

  ***Graphic Equalizer*** -- 6-band graphic equalizers: Low Shelf, 4x mid-bands, High Shelf.  Wider-band response hard-coded for zero ripple when all controls are at maximum or minimum.  This also means each band has more spill-over into adjacent bands but this creates a more gentle frequency response curve that seems to be a good fit for balancing electric guitar tones.

## Ambience

  ***Delay (echo)*** -- includes envelope control options for almost every user-accessible paramter.

  ***Reverb*** -- (http://kokkinizita.linuxaudio.org/ Zita Rev1)
