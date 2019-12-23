/********************************************************************************
 *     Program: REALFFTF.C
 *      Author: Philip VanBaren
 *        Date: 2 September 1993
 *
 * Description: These routines perform an FFT on real data.
 *              On a 486/33 compiled using Borland C++ 3.1 with full
 *              speed optimization and a small memory model, a 1024 point
 *              FFT takes about 16ms.
 *              This code is for floating point data.
 *
 *  Note: Output is BIT-REVERSED! so you must use the BitReversed to
 *        get legible output, (i.e. Real_i = buffer[ BitReversed[i] ]
 *                                  Imag_i = buffer[ BitReversed[i]+1 ] )
 *        Input is in normal order.
 ********************************************************************************/

#ifndef REALFFTF_H
#define REALFFTF_H

typedef float fft_type;

extern int *bit_reversed;

void initialize_FFT(int);
void end_FFT(void);
void real_FFT(fft_type *);

#endif
