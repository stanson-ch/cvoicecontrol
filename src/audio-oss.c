/***************************************************************************
                          audio.c  -  handle audio device
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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <string.h>
#include <stdio.h>
#include <glob.h>
#include <math.h>

#include "audio.h"
#include "preprocess.h"
#include "keypressed.h"

signed short rec_level, stop_level, silence_level;
int   fd_audio;
char *dev_audio;

static int is_open = 0;

/********************************************************************************
 * set name of audio device
 ********************************************************************************/

void setAudio(char *dev)
{
  char *prefix = "/dev/";

  if (dev == NULL)
    return;

  if (dev_audio != NULL)
    free(dev_audio);

  if (strncmp(dev, prefix, 5) == 0)
  {
    dev_audio = malloc(strlen(dev) + 1);
    strcpy(dev_audio, dev);
  }
  else
  {
    dev_audio = malloc(strlen(prefix) + strlen(dev) + 1);
    strcpy(dev_audio,                prefix);
    strcpy(dev_audio+strlen(prefix), dev);
  }

  is_open = 0;
}

/********************************************************************************
 * deselect any selected audio device
 ********************************************************************************/

void noAudio()
{
  if (dev_audio != NULL)
  {
    if (is_open)
      closeAudio();
    free(dev_audio);
  }

  dev_audio = NULL;
}

/********************************************************************************
 * return name of audio device
 ********************************************************************************/

char *getAudio()
{
  return(dev_audio);
}

/********************************************************************************
 * check whether device name has been set
 ********************************************************************************/

int audioOK()
{
  if (dev_audio != NULL)
    return AUDIO_OK;
  else
    return AUDIO_ERR;
}

/********************************************************************************
 * check audio capabilities and initialize audio
 ********************************************************************************/

int initAudio()
{
  int mask, fd;

  int channelsT = CHANNELS;
  int rateT     = RATE;
  int afmtT     = AFMT;
  int frag      = FRAG;

  if (dev_audio == NULL)
    return AUDIO_ERR;

  /***** open audio device */

  if ((fd = open(dev_audio, O_RDONLY, 0)) == -1)
    return(AUDIO_ERR);

  /***** check whether 16 bit recording is possible on the current sound hardware */

  if (ioctl(fd, SNDCTL_DSP_GETFMTS, &mask) == -1)
    return(AUDIO_ERR);
  if (!(mask & AFMT))
    return(AUDIO_ERR);

  /***** set audio recording to 16kHz, 16bit, mono */

  if (ioctl(fd, SNDCTL_DSP_SETFMT, &afmtT)      == -1 ||
      afmtT != AFMT                                   ||
      ioctl(fd, SNDCTL_DSP_CHANNELS, &channelsT)== -1 ||
      channelsT != CHANNELS                           ||
      ioctl(fd, SNDCTL_DSP_SPEED, &rateT)       == -1 ||
      rateT != RATE                                   ||
      ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag)  == -1)
    return(AUDIO_ERR);

  close(fd);
  is_open = 0;

  return(AUDIO_OK);
}

/********************************************************************************
 * find list of available audio devices
 ********************************************************************************/

AudioDevices *scanAudioDevices()
{
  int i,j;
  int mask, fd;
  glob_t result;
  AudioDevices *devices;

  if (glob("/dev/dsp*", 0, NULL, &result) != 0)
    return NULL;
  if (result.gl_pathc < 1)
    return NULL;

  devices       = malloc(sizeof(AudioDevices));
  devices->name = malloc((sizeof (char *))*result.gl_pathc);

  for (i = 0, j = 0; i < result.gl_pathc; i++)
  {
    int channelsT = CHANNELS;
    int rateT     = RATE;
    int afmtT     = AFMT;
    int frag      = FRAG;

    /***** scan audio device */

    if ((fd = open(result.gl_pathv[i], O_RDONLY, 0)) != -1 &&
	ioctl(fd, SNDCTL_DSP_GETFMTS, &mask)         != -1 &&
	(mask & AFMT)                                      &&
	ioctl(fd, SNDCTL_DSP_SETFMT, &afmtT)         != -1 &&
	afmtT == AFMT                                      &&
	ioctl(fd, SNDCTL_DSP_CHANNELS, &channelsT)   != -1 &&
	channelsT == CHANNELS                              &&
	ioctl(fd, SNDCTL_DSP_SPEED, &rateT)          != -1 &&
	rateT == RATE                                      &&
	ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag)     != -1)
    {
      devices->name[j] = malloc(strlen(result.gl_pathv[i])-5+1);
      strcpy(devices->name[j], result.gl_pathv[i]+5);
      j++;
    }
    close(fd);
  }

  devices->count = j;

  return devices;
}

/********************************************************************************
 * connect to audio device (recording)
 ********************************************************************************/

int openAudio()
{
  int mask;

  int channelsT = CHANNELS;
  int rateT     = RATE;
  int afmtT     = AFMT;
  int frag      = FRAG;

  if (dev_audio == NULL)
    return AUDIO_ERR;

  /***** open audio device */

  if ((fd_audio = open(dev_audio, O_RDONLY, 0)) == -1)
    return(AUDIO_ERR);

  /***** check whether 16 bit recording is possible on the current sound hardware */

  if (ioctl(fd_audio, SNDCTL_DSP_GETFMTS, &mask) == -1)
    return(AUDIO_ERR);
  if (!(mask & AFMT))
    return(AUDIO_ERR);

  /***** set audio recording to 16kHz, 16bit, mono */

  if (ioctl(fd_audio, SNDCTL_DSP_SETFMT, &afmtT)      == -1 ||
      afmtT != AFMT                                   ||
      ioctl(fd_audio, SNDCTL_DSP_CHANNELS, &channelsT)== -1 ||
      channelsT != CHANNELS                           ||
      ioctl(fd_audio, SNDCTL_DSP_SPEED, &rateT)       == -1 ||
      rateT != RATE                                   ||
      ioctl(fd_audio, SNDCTL_DSP_SETFRAGMENT, &frag)  == -1)
    return(AUDIO_ERR);

  is_open = 1;

  return(AUDIO_OK);
}

/********************************************************************************
 * disconnect from audio device (recording)
 ********************************************************************************/

int closeAudio()
{
  if (is_open)
  {
    close(fd_audio);
    is_open = 0;
  }
  return(AUDIO_OK);
}

/********************************************************************************
 * get maximum value of a block of recorded audio data
 ********************************************************************************/

int getBlockMax()
{
  /*****
   * buffer of size 'frag_size' that contains (raw) data which
   *****/

  unsigned char buffer_raw[FRAG_SIZE];
  signed short max = 0;
  int i;

  if (read(fd_audio, buffer_raw, FRAG_SIZE) != FRAG_SIZE)
    return -1;

  /***** retrieve max value */

  for (i = 0; i < FRAG_SIZE/2 - 1; i += 2)
  {
    signed short value = abs((signed short)(buffer_raw[i]|(buffer_raw[i+1]<<8)));
    if (value > max)
      max = value;
  }

  return max;
}

/********************************************************************************
 * estimate channel characteristic (channel mean vector)
 ********************************************************************************/

int calculateChannelMean()
{
  float frame[FFT_SIZE];            /***** data containers used for fft calculation */
  int   frames_N;                   /***** number of whole (overlapping) frames in current waveform buffer */
  int   frameI, i;                  /***** counter variables */
  float feat_vector[FEAT_VEC_SIZE]; /***** preprocessed feature vector */

  int N = 100;

  unsigned char buffer[N*FRAG_SIZE];

  for (i = 0; i < FEAT_VEC_SIZE; i++)
    channel_mean[i] = 0;

  for (i = 0; i < N; i++)
    if (read(fd_audio, buffer+i*FRAG_SIZE, FRAG_SIZE) != FRAG_SIZE)
      return AUDIO_ERR;

  /***** do preprocessing and calculate mean vector */

  initPreprocess();
  do_mean_sub = 0;

  /*****
   * number of frames that can be extracted from the current amount
   * of audio data
   *****/
  frames_N = (N*FRAG_SIZE-FFT_SIZE_CHAR)/OFFSET + 1;

  /***** extract these frames: */

  for (frameI = 0; frameI < frames_N; frameI++)
  {
    /***** gather frame */

    for (i = 0; i < FFT_SIZE; i++)
      frame[i] = ((float)((signed short)(buffer[frameI*OFFSET+2*i]|(buffer[frameI*OFFSET+2*i+1]<<8)))) *
	hamming_window[i];

    preprocessFrame(frame, feat_vector);

    for (i = 0; i < FEAT_VEC_SIZE; i++)
      channel_mean[i] += feat_vector[i];
  }

  for (i = 0; i < FEAT_VEC_SIZE; i++)
    channel_mean[i] /= frames_N;

  /***** cleanup */

  endPreprocess();

  return (AUDIO_OK);
}

/********************************************************************************
 * return current channel mean vector
 ********************************************************************************/

const float *getChannelMean()
{
 return channel_mean;
}

/********************************************************************************
 * playback utterance to audio device
 ********************************************************************************/

int playUtterance(unsigned char *wav, int length)
{
  int mask;

  int channelsT = CHANNELS;
  int rateT     = RATE;
  int afmtT     = AFMT;
  int frag      = FRAG;

  if (dev_audio == NULL)
    return AUDIO_ERR;

  /***** open audio device */

  if ((fd_audio = open(dev_audio, O_WRONLY, 0)) == -1)
    return(AUDIO_ERR);

  /***** check whether 16 bit recording is possible on the current sound hardware */

  if (ioctl(fd_audio, SNDCTL_DSP_GETFMTS, &mask) == -1)
    return(AUDIO_ERR);
  if (!(mask & AFMT))
    return(AUDIO_ERR);

  /***** set audio recording to 16kHz, 16bit, mono */

  if (ioctl(fd_audio, SNDCTL_DSP_SETFMT, &afmtT)      == -1 ||
      afmtT != AFMT                                   ||
      ioctl(fd_audio, SNDCTL_DSP_CHANNELS, &channelsT)== -1 ||
      channelsT != CHANNELS                           ||
      ioctl(fd_audio, SNDCTL_DSP_SPEED, &rateT)       == -1 ||
      rateT != RATE                                   ||
      ioctl(fd_audio, SNDCTL_DSP_SETFRAGMENT, &frag)  == -1)
    return(AUDIO_ERR);

  write(fd_audio, wav, length);

  close(fd_audio);
  return(AUDIO_OK);
}

/********************************************************************************
 * get an utterance from the sound card via auto recording
 ********************************************************************************/

unsigned char *getUtterance(int *length)
{
  int i;
  signed short max = 0;

  /***** set prefetch buffer size to 5, and allocate memory */

  int           prefetch_N    = 5;
  int           prefetch_pos  = 0;
  unsigned char prefetch_buf[FRAG_SIZE*prefetch_N];
  void         *prefetch      = prefetch_buf;

  int count     = 0;

  /***** space for one audio block */

  unsigned char buffer_raw[FRAG_SIZE];

  /***** store whole wav data in a queue-like buffer */

  struct Buffer
  {
    unsigned char buffer[FRAG_SIZE];
    struct Buffer *next;
  };

  int nr_of_blocks = 0;

  struct Buffer *first = NULL;
  struct Buffer *last  = NULL;

  unsigned char *return_buffer;

  initKeyPressed();

  memset (prefetch_buf, 0x00, FRAG_SIZE*prefetch_N);

  /***** prefetch data in a circular buffer, and check it for speech content */

  while (count < CONSECUTIVE_SPEECH_BLOCKS_THRESHOLD)
  {
    if (keyPressed())
    {
      return_buffer = NULL;
      goto getUtteranceReturn;
    }

    /* fprintf(stderr, "%d ", fgetc_unlocked(stdin)); */

    if (read(fd_audio, buffer_raw, FRAG_SIZE) != FRAG_SIZE)
    {
      return_buffer = NULL;
      goto getUtteranceReturn;
    }

    memcpy((prefetch+prefetch_pos*(FRAG_SIZE)), buffer_raw, FRAG_SIZE);
    prefetch_pos = (prefetch_pos+1)%prefetch_N;

    /***** check for speech */

    max = 0;
    for (i = 0; i < FRAG_SIZE/2 - 1; i += 2)
    {
      signed short value = abs((signed short)(buffer_raw[i]|(buffer_raw[i+1]<<8)));

      if (value > max)
	max = value;
    }
    if (max > rec_level)
      count++;
    else
      count = 0;
  }

  /***** store prefetch buffer in queue and do recording until level falls below threshold */

  for (i = prefetch_pos; i < prefetch_pos+prefetch_N; i++)
  {
    struct Buffer *data = (struct Buffer *)malloc(sizeof(struct Buffer));

    memcpy(data->buffer, prefetch+(i%prefetch_N)*FRAG_SIZE, FRAG_SIZE);
    data->next = NULL;
    nr_of_blocks++;

    if (last != NULL)
      last->next = data;
    last = data;
    if (first == NULL)
      first = data;
  }

  count = 0;
  while (count < CONSECUTIVE_NONSPEECH_BLOCKS_THRESHOLD)
  {
    struct Buffer *data = (struct Buffer *)malloc(sizeof(struct Buffer));

    if (keyPressed())
    {
      return_buffer = NULL;
      goto getUtteranceReturn;
    }

    if (read(fd_audio, data->buffer, FRAG_SIZE) != FRAG_SIZE)
    {
      free(data);
      return_buffer = NULL;
      goto getUtteranceReturn;
    }
    data->next = NULL;
    nr_of_blocks++;

    if (last != NULL)
      last->next = data;
    last = data;

    /***** check for nonspeech */

    max = 0;
    for (i = 0; i < FRAG_SIZE/2 - 1; i += 2)
    {
      signed short value = abs((signed short)(data->buffer[i]|(data->buffer[i+1]<<8)));

      if (value > max)
	max = value;
    }
    if (max < stop_level)
      count++;
    else
      count = 0;
  }

  /***** assemble data into one buffer, return it */

  return_buffer = (unsigned char *)malloc(nr_of_blocks*FRAG_SIZE);

  *length = nr_of_blocks*FRAG_SIZE;

  {
    struct Buffer *tmp_buffer = first;

    for (i = 0; i < nr_of_blocks; i++)
    {
      memcpy(return_buffer+(i*FRAG_SIZE), tmp_buffer->buffer, FRAG_SIZE);
      tmp_buffer = tmp_buffer->next;
    }
  }

  getUtteranceReturn:

  for (i = 0; i < nr_of_blocks; i++)
  {
    struct Buffer *tmp_buffer = first;
    first = first->next;
    free(tmp_buffer);
  }

  endKeyPressed();
  return return_buffer;
}

/********************************************************************************
 * preprocess an utterance
 ********************************************************************************/

float **preprocessUtterance(unsigned char *wav, int wav_length, int *prep_length)
{
  float frame[FFT_SIZE];            /***** data containers used for fft calculation */
  int   frames_N;                   /***** number of whole (overlapping) frames in current waveform buffer */
  int   frameI, i;                  /***** counter variables */
  float feat_vector[FEAT_VEC_SIZE]; /***** preprocessed feature vector */

  float **return_buffer;

  initPreprocess();

  /*****
   * number of frames that can be extracted from the current amount
   * of audio data
   *****/
  frames_N = (wav_length-FFT_SIZE_CHAR)/OFFSET + 1;
  return_buffer = (float **)malloc(sizeof(float *)*frames_N);
  *prep_length = frames_N;

  /***** extract these frames: */

  for (frameI = 0; frameI < frames_N; frameI++)
  {
    /***** gather frame */

    for (i = 0; i < FFT_SIZE; i++)
      frame[i] = ((float)((signed short)(wav[frameI*OFFSET+2*i]|(wav[frameI*OFFSET+2*i+1]<<8)))) *
	hamming_window[i];

    preprocessFrame(frame, feat_vector);

    for (i = 0; i < FEAT_VEC_SIZE; i++)
      feat_vector[i] -= channel_mean[i];

    return_buffer[frameI] = (float *)malloc(sizeof(float)*FEAT_VEC_SIZE);
    memcpy(return_buffer[frameI], feat_vector, sizeof(float)*FEAT_VEC_SIZE);
  }

  /***** cleanup */

  endPreprocess();

  return (return_buffer);
}

