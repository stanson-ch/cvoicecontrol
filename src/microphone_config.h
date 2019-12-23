/***************************************************************************
                          microphone_config.h  -  microphone calibration
                          												tool
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

#include "mixer.h"
#include "audio.h"

#ifndef MICROPHONE_CONFIG_H
#define MICROPHONE_CONFIG_H

enum state {inactive, invalid, ok};

MixerDevices *mixer_devices;
AudioDevices *audio_devices;

#define MIN2(a,b) ((a<b)?(a):(b))

/*****
  fraction of max level (=32768) below which the signal is
  considered to represent silence
  *****/
float silence    = 0.02;
/*****
  fraction of max level (=32768) below which the signal is
  considered to represent (too) low volume speech
  *****/
float low_volume = 0.15;
/*****
  fraction of max level (=32768) above which the signal is
  considered to represent (too) high volume speech
  *****/
float high_volume = 0.75;

#define MIN_REASONABLE_MIC_LEVEL   20
#define MAX_REASONABLE_IGAIN_LEVEL 50

#endif
