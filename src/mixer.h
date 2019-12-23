/***************************************************************************
                          mixer.h  -  handle mixer device
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

#ifndef MIXER_H
#define MIXER_H

typedef struct
{
  int    count;  /***** number of available devices */
  char **name;   /***** list of device names */
} MixerDevices;

void noMixer();
void setMixer(char *dev);
int  mixerOK();
const char *getMixer();
int  initMixer();
MixerDevices *scanMixerDevices();

int mixerHasIGain();

int setIGainLevel(int level);
int setMicLevel(int level);

#define MIXER_ERR -1
#define MIXER_OK  1

extern int mic_level;
extern int igain_level;

#endif
