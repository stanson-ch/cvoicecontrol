/***************************************************************************
                          audio.h  -  handle audio device
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

#ifndef AUDIO_H
#define AUDIO_H

#define CHANNELS  1
#define RATE      16000
#define AFMT      AFMT_S16_LE
/***** unlimited number of fragments of size 2^11 = 256 bytes */
#define FRAG      0x7fff000b
#define FRAG_SIZE 2048

#define AUDIO_ERR 1
#define AUDIO_OK  0

#define CONSECUTIVE_SPEECH_BLOCKS_THRESHOLD    3
#define CONSECUTIVE_NONSPEECH_BLOCKS_THRESHOLD 5


/********************************************************************************
 * For now, microphone input level is used to start and stop recording
 * (it would be better to do a fast spectral speech/nonspeech analysis)
 *
 * rec_level      input level above which recording is started
 * stop_level     input level below which recording is stopped
 * silence_level  average micro level of silence
 ********************************************************************************/

extern int   fd_audio;
extern signed short rec_level, stop_level, silence_level;

typedef struct
{
  int count;   /***** number of available devices */
  char **name; /***** list of device names */
} AudioDevices;

void noAudio();
void setAudio(char *dev);
int  audioOK();
char *getAudio();
int  initAudio();
AudioDevices *scanAudioDevices();

int openAudio();
int getBlockMax();
unsigned char *getUtterance(int *length);
int playUtterance(unsigned char *wav, int length);
float **preprocessUtterance(unsigned char *wav, int wav_length, int *prep_length);
int calculateChannelMean();
const float *getChannelMean();
int closeAudio();

#endif
