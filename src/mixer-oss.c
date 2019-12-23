/***************************************************************************
                          mixer.c  -  handle mixer device
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
#include <math.h>
#include <stdio.h>
#include <glob.h>

#include "mixer.h"

int mic_level;
int igain_level;
static char *dev_mixer = NULL;

/********************************************************************************
 * set mixer device name
 ********************************************************************************/

void setMixer(char *dev)
{
  char *prefix = "/dev/"; /***** prefix of directory where device files are located */

  if (dev == NULL)
    return;

  if (dev_mixer != NULL) /***** free memory of previously set mixer device */
    free(dev_mixer);

  if (strncmp(dev, prefix, 5) == 0) /***** if dev is already of type /dev/... just strcpy it */
  {
    dev_mixer = malloc(strlen(dev) + 1);
    strcpy(dev_mixer, dev);
  }
  else /***** otherwise prepend the prefix defined above */
  {
    dev_mixer = malloc(strlen(prefix) + strlen(dev) + 1);
    strcpy(dev_mixer, prefix);
    strcat(dev_mixer, dev);
  }
}

/********************************************************************************
 * deselect any selected mixer device
 ********************************************************************************/

void noMixer()
{
  if (dev_mixer != NULL) /***** free memory of previously set mixer device */
    free(dev_mixer);

  dev_mixer = NULL;
}

/********************************************************************************
 * get mixer device name
 ********************************************************************************/

const char *getMixer()
{
  return(dev_mixer); /***** return value may be NULL, if no mixer is set! */
}

/********************************************************************************
 * tells whether the mixer variable has been set
 ********************************************************************************/

int mixerOK()
{
  if (dev_mixer != NULL)
    return MIXER_OK;
  else
    return MIXER_ERR;
}

/********************************************************************************
 * tells whether the mixer has an input gain channel
 ********************************************************************************/

int mixerHasIGain()
{
  int mask_mixer, fd;

  return MIXER_ERR;
  /***** open mixer device */

  if ((fd = open(dev_mixer, O_RDWR, 0)) == -1)
    return(MIXER_ERR);

  /***** check whether mic and igain devices available */

  if (ioctl(fd, SOUND_MIXER_READ_DEVMASK, &mask_mixer) == -1)
    return(MIXER_ERR);

  if (!(mask_mixer & SOUND_MASK_IGAIN))
    return(MIXER_ERR);

  close(fd);

  return(MIXER_OK);
}


/********************************************************************************
 * check mixer capabilities and initialize mixer
 ********************************************************************************/

int initMixer()
{
  int mask_mixer, fd;

  /***** open mixer device */

  if ((fd = open(dev_mixer, O_RDWR, 0)) == -1)
    return(MIXER_ERR);

  /***** check whether mic and igain devices available */

  if (ioctl(fd, SOUND_MIXER_READ_DEVMASK, &mask_mixer) == -1)
    return(MIXER_ERR);

  if (!(mask_mixer & SOUND_MASK_MIC))
    return(MIXER_ERR);

  /*
  if (!(mask_mixer & SOUND_MASK_RECLEV))
    return(MIXER_ERR);
  */

  /***** check whether available recording sources include microphone */

  if (ioctl(fd, SOUND_MIXER_READ_RECMASK, &mask_mixer) == -1)
    return(MIXER_ERR);

  if (!(mask_mixer & SOUND_MASK_MIC))
    return(MIXER_ERR);

  /***** set microphone as active recording channel */

  if (ioctl(fd, SOUND_MIXER_READ_RECSRC, &mask_mixer) == -1)
    return(MIXER_ERR);

  if (!(mask_mixer & SOUND_MASK_MIC))
  {
    mask_mixer = SOUND_MASK_MIC;
    if (ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &mask_mixer) == -1)
      return(MIXER_ERR);
    if (ioctl(fd, SOUND_MIXER_READ_RECSRC, &mask_mixer) == -1)
      return(MIXER_ERR);
    if (!(mask_mixer & SOUND_MASK_MIC))
      return(MIXER_ERR);
  }

  close(fd);

  return(MIXER_OK);
}

/********************************************************************************
 * return a list of available mixer devices
 ********************************************************************************/

MixerDevices *scanMixerDevices()
{
  int i,j;            /***** counter variables */
  int mask_mixer, fd; /***** mixer device related variables */
  glob_t result;      /***** result of the glob() call */

  MixerDevices *devices; /***** temporary variable */

  /***** get a list of device names that fit the pattern /dev/mixer*  */

  if (glob("/dev/mixer*", 0, NULL, &result) != 0)
    return NULL;
  if (result.gl_pathc < 1)
    return NULL;

  /*****
   * allocate memory for the structure that will contain
   * the information about the list of available mixer devices
   *****/
  devices       = malloc(sizeof(MixerDevices));
  devices->name = malloc((sizeof (char *))*result.gl_pathc);

  /***** check each mixer device whether it is working properly */

  for (i = 0, j = 0; i < result.gl_pathc; i++)
  {
    /***** scan abilities of current mixer device */

    if ((fd = open(result.gl_pathv[i], O_RDWR, 0)) != -1       &&
	ioctl(fd, SOUND_MIXER_READ_DEVMASK, &mask_mixer) != -1 &&
	(mask_mixer & SOUND_MASK_MIC)   &&
	/* (mask_mixer & SOUND_MASK_IGAIN) && */
	ioctl(fd, SOUND_MIXER_READ_RECMASK, &mask_mixer) != -1 &&
	(mask_mixer & SOUND_MASK_MIC))
    {
      /***** if mixer device looks ok add it to the list */

      devices->name[j] = malloc(strlen(result.gl_pathv[i])-5+1);
      strcpy(devices->name[j], result.gl_pathv[i]+5);
      j++;
    }

    close(fd);
  }

  devices->count = j; /***** number of ok looking mixer devices */

  return devices; /***** return the information */
}

/********************************************************************************
 * set level of microphone channel
 ********************************************************************************/

int setMicLevel(int level)
{
  int fd, mask; /***** mixer related variables */

  /***** only accept reasonable mixer values */

  if (level < 0 || level > 99)
    return(MIXER_ERR);

  /***** open mixer devices */

  if ((fd = open(dev_mixer, O_RDWR, 0)) == -1)
    return(MIXER_ERR);

  mask = (level<<8)|level; /***** set left and right channel to the same value */

  if (ioctl(fd, SOUND_MIXER_WRITE_MIC, &mask) == -1)
    return(MIXER_ERR);

  close(fd);

  mic_level = level; /***** remember mixer level */
  return(MIXER_OK);
}

/********************************************************************************
 * set level of input gain channel
 ********************************************************************************/

int setIGainLevel(int level)
{
  int fd, mask; /***** mixer related variables */

  /***** only accept reasonable mixer values */

  if (level < 1 || level > 99)
    return(MIXER_ERR);

  /***** open mixer devices */

  if ((fd = open(dev_mixer, O_RDWR, 0)) == -1)
    return(MIXER_ERR);

  mask = (level<<8)|level; /***** set left and right channel to the same value */

  if (ioctl(fd, SOUND_MIXER_WRITE_IGAIN, &mask) == -1)
    return(MIXER_ERR);

  close(fd);

  igain_level = level; /***** remember mixer level */
  return(MIXER_OK);
}
