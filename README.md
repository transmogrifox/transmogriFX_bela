# transmogriFX_bela
Audio Multi-Effects processor for Bela (http://bela.io)

Code is released to the public domain under the UNLICENSE
For more information, please refer to <http://unlicense.org/>

Contains code for a multi-effects processor for Bela.  Designed with default settings optimized for guitar use, but not limited strictly to guitar applications.

The contained code is not designed to compile and run outside of the Bela core system.  However, the individual effects files are mostly self-contained and do not rely on a web of dependencies, making it easy to fork the individual audio effects for inclusion in other projects. In case of code re-use elsewhere, the Bela-specific file Render.cpp can be studied for example usage.

Description
-------------
Current effects in project:

+++Dynamics+++

  Dynamic Range Compressor -- includes typical pro compressor controls: 
      Threshold, Ratio, Attack, Release, Gain, Soft/Hard Knee, Mix(Parallel compression)
      
  Sustainer -- Simplified 2-knob compressor.  Is also more "open" sounding than the traditional compressor.

+++Modulation+++

  Chorus -- includes envelope control options for almost every user-accessible paramter.
  
  Flanger -- includes envelope control options for almost every user-accessible paramter.
  
  Phaser
  
  Tremolo

+++Filters+++
  
  Wah Wah -- Inductor wah DSP Model (based on classic Vox and Crybaby circuits)
  
  State Variable filter -- Implements Envelope and sequenced ADSR control

+++Misc+++

  Delay (echo) -- includes envelope control options for almost every user-accessible paramter.
  
  Reverb (http://kokkinizita.linuxaudio.org/ Zita Rev1)
  
  2x 6-band graphic equalizers
  
