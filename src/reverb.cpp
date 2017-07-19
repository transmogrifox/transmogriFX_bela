// -----------------------------------------------------------------------
//
//  Copyright (C) 2003-2010 Fons Adriaensen <fons@linuxaudio.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// -----------------------------------------------------------------------


#include <stdio.h>
#include <string.h>
#include <math.h>
#include "reverb.h"


// -----------------------------------------------------------------------


Diff1::Diff1 (void) :
    _size (0),
    _line (0)
{
}


Diff1::~Diff1 (void)
{
    fini ();
}


void Diff1::init (int size, float c)
{
    _size = size;
    _line = new float [size];
    memset (_line, 0, size * sizeof (float));
    _i = 0;
    _c = c;
}


void Diff1::fini (void)
{
    delete[] _line;
    _size = 0;
    _line = 0;
}


// -----------------------------------------------------------------------


Delay::Delay (void) :
    _size (0),
    _line (0)
{
}


Delay::~Delay (void)
{
    fini ();
}


void Delay::init (int size)
{
    _size = size;
    _line = new float [size];
    memset (_line, 0, size * sizeof (float));
    _i = 0;
}


void Delay::fini (void)
{
    delete[] _line;
    _size = 0;
    _line = 0;
}


// -----------------------------------------------------------------------


Vdelay::Vdelay (void) :
    _size (0),
    _line (0)
{
}


Vdelay::~Vdelay (void)
{
    fini ();
}


void Vdelay::init (int size)
{
    _size = size;
    _line = new float [size];
    memset (_line, 0, size * sizeof (float));
    _ir = 0;
    _iw = 0;
}


void Vdelay::fini (void)
{
    delete[] _line;
    _size = 0;
    _line = 0;
}


void Vdelay::set_delay (int del)
{
    _ir = _iw - del;
    if (_ir < 0) _ir += _size;
}


// -----------------------------------------------------------------------


void Filt1::set_params (float del, float tmf, float tlo, float wlo, float thi, float chi)
{
    float g, t;

    _gmf = powf (0.001f, del / tmf);
    _glo = powf (0.001f, del / tlo) / _gmf - 1.0f;
    _wlo = wlo;
    g = powf (0.001f, del / thi) / _gmf;
    t = (1 - g * g) / (2 * g * g * chi);
    _whi = (sqrtf (1 + 4 * t) - 1) / (2 * t);
}


// -----------------------------------------------------------------------


float Reverb::_tdiff1 [8] =
{
    20346e-6f,
    24421e-6f,
    31604e-6f,
    27333e-6f,
    22904e-6f,
    29291e-6f,
    13458e-6f,
    19123e-6f
};


float Reverb::_tdelay [8] =
{
   153129e-6f,
   210389e-6f,
   127837e-6f,
   256891e-6f,
   174713e-6f,
   192303e-6f,
   125000e-6f,
   219991e-6f
};


Reverb::Reverb (void)
{
}


Reverb::~Reverb (void)
{
    fini ();
}


void Reverb::init (float fsamp, bool ambis, size_t frame_size)
{
    int i, k1, k2;
    inp[0] = new float[frame_size];
    inp[1] = new float[frame_size];
    out_mono = new float[frame_size];
    for(i=0; i < 4; i++)
    {
        out[i] = new float[frame_size];
    }
    for(i=0; i < frame_size; i++)
    {
        inp[0][i] = 0.0;
        inp[1][i] = 0.0;
        out_mono[i] = 0.0;
        for(k1 = 0; k1 < 4; k1++)
        {
            out[k1][i] = 0.0;
        }
    }
    _fragm = 1024;
    _nsamp = 0;

    _fsamp = fsamp;
    _ambis = ambis;
    _cntA1 = 1;
    _cntA2 = 0;
    _cntB1 = 1;
    _cntB2 = 0;
    _cntC1 = 1;
    _cntC2 = 0;

    _ipdel = 0.02f;
    _xover = 800.0f;
    _rtlow = 2.0f;
    _rtmid = 3.0f;
    _fdamp = 6.5e3f;
    _opmix = 0.25f;
    _rgxyz = 0.0f;

    _g0 = _d0 = 0;
    _g1 = _d1 = 0;

    _vdelay0.init ((int)(0.1f * _fsamp));
    _vdelay1.init ((int)(0.1f * _fsamp));
    for (i = 0; i < 8; i++)
    {
	k1 = (int)(floorf (_tdiff1 [i] * _fsamp + 0.5f));
	k2 = (int)(floorf (_tdelay [i] * _fsamp + 0.5f));
        _diff1 [i].init (k1, (i & 1) ? -0.6f : 0.6f);
        _delay [i].init (k2 - k1);
    }

    _pareq1.setfsamp (fsamp);
    _pareq2.setfsamp (fsamp);
}


void Reverb::fini (void)
{
    for (int i = 0; i < 8; i++) _delay [i].fini ();
}


void Reverb::prepare (int nfram)
{
    int    a, b, c, i, k;
    float  t0, t1, wlo, chi;

    a = _cntA1;
    b = _cntB1;
    c = _cntC1;
    _d0 = _d1 = 0;

    if (a != _cntA2)
    {
	if (_ipdel < 0.02f) _ipdel = 0.02f;
	if (_ipdel > 0.10f) _ipdel = 0.10f;
        k = (int)(floorf ((_ipdel - 0.02f) * _fsamp + 0.5f));
        _vdelay0.set_delay (k);
        _vdelay1.set_delay (k);
        _cntA2 = a;
    }

    if (b != _cntB2)
    {
	if (_xover < 50.0f)  _xover = 50.0f;
	if (_xover > 1.0e3f) _xover = 1.0e3f;
	if (_rtlow < 1.0f) _rtlow = 1.0f;
	if (_rtlow > 8.0f) _rtlow = 8.0f;
	if (_rtmid < 1.0f) _rtmid = 1.0f;
	if (_rtmid > 8.0f) _rtmid = 8.0f;
	if (_fdamp <  1.5e3f) _fdamp =  1.5e3f;
	if (_fdamp > 24.0e3f) _fdamp = 24.0e3f;
         wlo = 6.2832f * _xover / _fsamp;
	 if (_fdamp > 0.49f * _fsamp) chi = 2;
	 else chi = 1 - cosf (6.2832f * _fdamp / _fsamp);
         for (i = 0; i < 8; i++)
	 {
             _filt1 [i].set_params (_tdelay [i], _rtmid, _rtlow, wlo, 0.5f * _rtmid, chi);
	 }
         _cntB2 = b;
    }

    if (c != _cntC2)
    {
        if (_rtmid < 1.0f) _rtmid = 1.0f;
        if (_rtmid > 8.0f) _rtmid = 8.0f;
	if (_ambis)
	{
	    if (_rgxyz < -9.0f) _rgxyz = -9.0f;
	    if (_rgxyz >  9.0f) _rgxyz =  9.0f;
	    t0 = 1.0f / sqrtf (_rtmid);
	    t1 = t0 * powf (10.0f, 0.05f * _rgxyz);
	}
	else
	{
	    if (_opmix < 0.0f) _opmix = 0.0f;
	    if (_opmix > 1.0f) _opmix = 1.0f;
	    t0 = (1 - _opmix) * (1 + _opmix);
	    t1 = 0.7f * _opmix * (2 - _opmix) / sqrtf (_rtmid);
	}
        _d0 = (t0 - _g0) / nfram;
        _d1 = (t1 - _g1) / nfram;
        _cntC2 = c;
    }

    _pareq1.prepare (nfram);
    _pareq2.prepare (nfram);

}


void Reverb::process (int nfram, float *inp [], float *out [])
{
    int   i, n;
    float *p0, *p1;
    float *q0, *q1;//, *q2, *q3;
    float t, g, x0, x1, x2, x3, x4, x5, x6, x7;

    g = sqrtf (0.125f);

    p0 = inp [0];
    p1 = inp [1];
    q0 = out [0];
    q1 = out [1];
//    q2 = out [2];
//    q3 = out [3];

    for (i = 0; i < nfram; i++)
    {
        _vdelay0.write (p0 [i]);
        _vdelay1.write (p1 [i]);

        t = 0.3f * _vdelay0.read ();
        x0 = _diff1 [0].process (_delay [0].read () + t);
        x1 = _diff1 [1].process (_delay [1].read () + t);
        x2 = _diff1 [2].process (_delay [2].read () - t);
        x3 = _diff1 [3].process (_delay [3].read () - t);
        t = 0.3f * _vdelay1.read ();
        x4 = _diff1 [4].process (_delay [4].read () + t);
        x5 = _diff1 [5].process (_delay [5].read () + t);
        x6 = _diff1 [6].process (_delay [6].read () - t);
        x7 = _diff1 [7].process (_delay [7].read () - t);

        t = x0 - x1; x0 += x1;  x1 = t;
        t = x2 - x3; x2 += x3;  x3 = t;
        t = x4 - x5; x4 += x5;  x5 = t;
        t = x6 - x7; x6 += x7;  x7 = t;
        t = x0 - x2; x0 += x2;  x2 = t;
        t = x1 - x3; x1 += x3;  x3 = t;
        t = x4 - x6; x4 += x6;  x6 = t;
        t = x5 - x7; x5 += x7;  x7 = t;
        t = x0 - x4; x0 += x4;  x4 = t;
        t = x1 - x5; x1 += x5;  x5 = t;
        t = x2 - x6; x2 += x6;  x6 = t;
        t = x3 - x7; x3 += x7;  x7 = t;

//        if (_ambis)
//        {
//            _g0 += _d0;
//            _g1 += _d1;
//            q0 [i] = _g0 * x0;
//            q1 [i] = _g1 * x1;
//            q2 [i] = _g1 * x4;
//            q3 [i] = _g1 * x2;
//        }
//        else
//        {
            _g1 += _d1;
            q0 [i] = _g1 * (x1 + x2);
            q1 [i] = _g1 * (x1 - x2);
//        }

        _delay [0].write (_filt1 [0].process (g * x0));
        _delay [1].write (_filt1 [1].process (g * x1));
        _delay [2].write (_filt1 [2].process (g * x2));
        _delay [3].write (_filt1 [3].process (g * x3));
        _delay [4].write (_filt1 [4].process (g * x4));
        _delay [5].write (_filt1 [5].process (g * x5));
        _delay [6].write (_filt1 [6].process (g * x6));
        _delay [7].write (_filt1 [7].process (g * x7));
    }

    //n = _ambis ? 4 : 2;
    n = 2;
    _pareq1.process (nfram, n, out);
    _pareq2.process (nfram, n, out);
//    if (!_ambis)
//    {
        for (i = 0; i < nfram; i++)
        {
            _g0 += _d0;
            q0 [i] += _g0 * p0 [i];
            q1 [i] += _g0 * p1 [i];
        }
//    }
}

void Reverb::tick_mono(int frames, float *audio)
{
    int   i, k;
    int fr = frames;
    float *inp_[2];
    inp_[0] = (float*) inp[0];
    inp_[1] = (float*) inp[1];
    float *out_[4];
    out_[0] = out[0];
    out_[1] = out[1];
    

    for(i=0; i < frames; i++)
    {
        inp_ [0][i] = audio[i];
        inp_ [1][i] = audio[i];
        out_ [0][i] = 0.0;
        out_ [1][i] = 0.0;
    }

    while (frames)
    {
        if (!_nsamp)
        {
            prepare (_fragm);
            _nsamp = _fragm;
        }
        k = (_nsamp < frames) ? _nsamp : frames;
        process (k, inp_, out_);

        for (i = 0; i < 2; i++) inp_ [i] += k;
        for (i = 0; i < 2; i++) out_ [i] += k;

        frames -= k;
        _nsamp -= k;
    }

    //Overwrite input buffer
    for(i=0; i < fr; i++)
    {
        out_mono[i] = (out [0][i] + out [1][i]);
        audio[i] = out_mono[i];
    }
}
// -----------------------------------------------------------------------
