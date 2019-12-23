/***************************************************************************
                          mixer-alsa.c  -  handle ALSA mixer device
                             -------------------
    begin                : Sun Dec 22 2019
    copyright            : (C) 2019 by Stanson
    email                : me@stanson.ch
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
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <glob.h>
#include <alsa/asoundlib.h>

#include "mixer.h"

int mic_level;
int igain_level;
static char *dev_mixer = NULL;

/********************************************************************************
 * set mixer device name
 ********************************************************************************/

void setMixer(char *dev)
{
    if( !dev ) return;
    if( dev_mixer ) free( dev_mixer );
    dev_mixer = strdup( dev );
}

/********************************************************************************
 * deselect any selected mixer device
 ********************************************************************************/

void noMixer()
{
    if( dev_mixer ) free(dev_mixer);
    dev_mixer = NULL;
}

/********************************************************************************
 * get mixer device name
 ********************************************************************************/

const char *getMixer()
{
    return dev_mixer;
}

/********************************************************************************
 * tells whether the mixer variable has been set
 ********************************************************************************/

int mixerOK()
{
    return dev_mixer ? MIXER_OK : MIXER_ERR;
}

/********************************************************************************
 * tells whether the mixer has an input gain channel
 ********************************************************************************/

int mixerHasIGain()
{
    return MIXER_ERR;
}


/********************************************************************************
 * check mixer capabilities and initialize mixer
 ********************************************************************************/

int initMixer()
{
    return MIXER_OK;
}

/********************************************************************************
 * return a list of available mixer devices
 ********************************************************************************/

MixerDevices *scanMixerDevices()
{
    MixerDevices *devices;
    devices          = malloc(sizeof(MixerDevices));
    devices->name    = malloc((sizeof (char *)));
    devices->name[0] = strdup( "default" );
    devices->count   = 1;
    return devices;
}

/********************************************************************************
 * set level of channel
 ********************************************************************************/
int setChannelLevel( int level, const char * name )
{
    int				ret;
    long int			lmin, lmax;
    snd_mixer_t			*mixer = NULL;
    snd_mixer_selem_id_t	*sid;
    snd_mixer_elem_t		*elem;

    if( !dev_mixer )
    {
	fprintf( stderr, "Mixer device is not set\n" );
	return MIXER_ERR;
    }

    if( level < 0 || level > 100 )
    {
	fprintf( stderr, "%s level out of range: %d\n", name, level );
	return MIXER_ERR;
    }

    if( ( ret = snd_mixer_open( &mixer, 0 ) ) < 0 ) goto out;
    if( ( ret = snd_mixer_attach( mixer, dev_mixer ) ) < 0 ) goto out;
    if( ( ret = snd_mixer_selem_register( mixer, NULL, NULL ) ) < 0 ) goto out;
    if( ( ret = snd_mixer_load( mixer ) ) < 0 ) goto out;
    snd_mixer_selem_id_alloca( &sid );
    snd_mixer_selem_id_set_index( sid, 0 );
    snd_mixer_selem_id_set_name( sid, name );
    if( ( elem = snd_mixer_find_selem( mixer, sid ) ) == NULL )
    {
	fprintf( stderr, "ALSA error: Element %s not found\n", name );
	ret = -1;
	goto out_close;
    }
    if( ( ret = snd_mixer_selem_get_capture_volume_range( elem, &lmin, &lmax ) ) < 0 ) goto out;
    if( ( ret = snd_mixer_selem_set_capture_volume_all( elem, lmin + level * ( lmax - lmin ) / 100 ) ) < 0 ) goto out;
out:
    if( ret < 0 ) fprintf( stderr, "ALSA error: %s\n", snd_strerror( ret ) );
out_close:
    if( mixer ) snd_mixer_close( mixer );
    return ret < 0 ? MIXER_ERR : MIXER_OK;
}

/********************************************************************************
 * set level of microphone channel
 ********************************************************************************/

int setMicLevel( int level )
{
    mic_level = level;
    return setChannelLevel( level, "Capture" );
}

/********************************************************************************
 * set level of input gain channel
 ********************************************************************************/

int setIGainLevel(int level)
{
//    return setChannelLevel( level, "Capture" );
    igain_level = level;
    return MIXER_OK;
}
