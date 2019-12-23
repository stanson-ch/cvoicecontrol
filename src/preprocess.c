/***************************************************************************
                          preprocess.c  -  Preprocessing of a wave file for
                          	           speech recognition
                             -------------------
    begin                : Sat Feb 12 2000
    copyright            : (C) 2000 by Daniel Kiecza
    email                : daniel@kiecza.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include<math.h>

#include "realfftf.h"
#include "preprocess.h"

/*****
  used for reduction of short-time spectrum to mel scale coefficients
  *****/
int   filter_banks[17];

/*****
  contains the power spectrum
  *****/
float power_spec[POWER_SPEC_SIZE];

/********************************************************************************
 * Hamming window width = 16ms ! (256 Frames)
 * (hamming_size == fft_size)
 *
 * a window function that a frame of audio values is multiplied with.
 * This is done to smoothen the beginning and end of a frame,
 * i.e. to reduce discontinuities at the two ends and as a result
 * to reduce the number of artefacts in the power spectrum
 ********************************************************************************/

float hamming_window[HAMMING_SIZE];

/*****
  The characteristics of the recording channel
  This is substracted from each feature vector to reduce
  channel effects
  *****/
int   do_mean_sub;
float channel_mean[FEAT_VEC_SIZE];

int i,j; /***** counter variables */

/********************************************************************************
 * initialize preprocessing s tuff
 ********************************************************************************/

int initPreprocess()
{
  float tmp;
  int i;

  /*****
   * the following is strictly speaking not necessary
   * as the channel mean will be read from the configuration file later on.
   *****/
  for (i = 0; i < VECSIZE; i++)
    channel_mean[i] = 0;

  /***** initialize external fft procedure */

  initialize_FFT(FFT_SIZE);

  /***** setup hamming window */

  tmp = 2.0*M_PI/(HAMMING_SIZE-1);
  for (i = 0; i < HAMMING_SIZE; i++)
    hamming_window[i] = 0.54 - 0.46*cos(tmp*i);

  /***** initialize mel scale filter bank */

  filter_banks[0]=0;
  filter_banks[1]=2;
  filter_banks[2]=6;
  filter_banks[3]=10;
  filter_banks[4]=14;
  filter_banks[5]=18;
  filter_banks[6]=22;
  filter_banks[7]=26;
  filter_banks[8]=30;
  filter_banks[9]=35;
  filter_banks[10]=41;
  filter_banks[11]=48;
  filter_banks[12]=57;
  filter_banks[13]=68;
  filter_banks[14]=81;
  filter_banks[15]=97;
  filter_banks[16]=116;

  do_mean_sub = 1; /***** turn substraction of channel mean vector on! */

  return 1; /***** return ok */
}

/********************************************************************************
 * preprocess a frame of audio data
 ********************************************************************************/

int preprocessFrame(float *frame, float *result)
{
  real_FFT(frame); /***** fast fourier transformation of the frame */

  /***** gather power spectrum from results */

  for (i = 0 ; i < POWER_SPEC_SIZE ; i++)
    power_spec[i] = frame[bit_reversed[i]]*frame[bit_reversed[i]]+
      frame[bit_reversed[i]+1]*frame[bit_reversed[i]+1];

  /***** mel scale reduction */

  for (i = 0; i < FEAT_VEC_SIZE; i++)
  {
    int from = (int)filter_banks[i  ];
    int to   = (int)filter_banks[i+1];

    if (from == 0)
      result[i] = power_spec[0];
    else
      result[i] = power_spec[from]/2.0;
    result[i] += power_spec[to]/2.0;
    for (j = from+1; j <= to-1; j++)
      result[i] += power_spec[j];
    result[i] = log(result[i] + 1.0)/M_LN2;

    /***** substraction of channel mean */

    if (do_mean_sub)
      result[i] = result[i] - channel_mean[i];
  }

  return 1;
}

/********************************************************************************
 * wrap up preprocessing component (free memory etc.)
 ********************************************************************************/

void endPreprocess()
{
  /***** cleanup */

  end_FFT();
}
