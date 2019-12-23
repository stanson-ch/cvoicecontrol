/***************************************************************************
                          microphone_config.c  -  microphone calibration
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

#define MAIN_C

#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "microphone_config.h"

#include "mixer.h"
#include "audio.h"
#include "preprocess.h"

#include "ncurses_tools.h"

#include "configuration.h"

/********************************************************************************
 * setHighlight
 ********************************************************************************/

void setHighlight(WINDOW *window, enum state s, bool current)
{
  wattroff(window, A_INVIS|A_NORMAL|A_BOLD);

  /***** current item is displayed in reverse mode */

  if (current)
    wattron(window, A_REVERSE);
  else
    wattroff(window, A_REVERSE);

  /*****
   * if an item is 'inactive', do not display it,
   * if it is 'invalid', show it in normal mode,
   * if it is 'ok', show it in bold!
   *****/
  switch (s)
  {
  case inactive: wattron(window, A_INVIS);  break;
  case invalid:  wattron(window, A_NORMAL); break;
  case ok:       wattron(window, A_BOLD);   break;
  }
}

/********************************************************************************
 * make sure that the user doesn't exit accidentially
 ********************************************************************************/

int safeExit()
{
  int retval = 0;  /***** return value */

  int width = 45, height = 11, i, j;               /***** ncurses related variables */
  WINDOW *resetscr = popupWindow (width, height);

  wattrset (resetscr, color_term ? COLOR_PAIR(3) | A_BOLD : A_REVERSE); /***** set bg color of the dialog */

  /*****
   * draw a box around the dialog window
   * and empty it.
   *****/
  werase(resetscr);
  box (resetscr, 0, 0);
  for (i = 1; i < width-1; i++)
    for (j = 1; j < height-1; j++)
      mvwaddch(resetscr, j, i, ' ');

  /***** dialog header */

  mvwaddstr(resetscr, 1, 2, "Warning!");
  mvwaddseparator(resetscr, 2, width);

  /***** dialog message */

  mvwaddstr(resetscr, 4, 2,"You did not finish the configuration");
  mvwaddstr(resetscr, 5, 2,"process and no data has been written");
  mvwaddstr(resetscr, 6, 2,"to your configuration file. Do you");
  mvwaddstr(resetscr, 7, 2,"really want to quit!? Press 'y' to quit,");
  mvwaddstr(resetscr, 8, 2,"any other key to continue .....");

  wmove(resetscr, 1, 11);  /***** set cursor to an appropriate location */
  wrefresh (resetscr);     /***** refresh the dialog */

  if (getch() == 'y') /***** if user hits 'y' */
    retval = 1;       /***** return ok */
  else
    retval = 0;       /***** any other key -> don't quit program! */

  delwin(resetscr);   /***** delete ncurses dialog window */
  return(retval);
}

/********************************************************************************
 * select mixer from list of available mixer devices
 ********************************************************************************/

int selectMixer()
{
  int top = 0, current = 0; /***** selection menu related variables */
  int max_view = 9;

  int retval = 0;           /***** return value */

  int request_finish = 0;   /***** leave current dialog, if request_finish is set to 1 */

  int width = 45, height = 15, i, j;               /***** ncurses related variables */
  WINDOW *mixerscr = popupWindow (width, height);

  while (!request_finish)
  {
    wattrset (mixerscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

    /*****
     * draw a box around the dialog window
     * and empty it.
     *****/
    werase (mixerscr);
    box (mixerscr, 0, 0);
    for (i = 1; i < width-1; i++)
      for (j = 1; j < height-1; j++)
	mvwaddch(mixerscr, j, i, ' ');

    /***** dialog header */

    mvwaddstr(mixerscr, 1, 2, "Select Mixer Device:");
    mvwaddseparator(mixerscr, 2, width);

    /***** selection area */

    for (i = 0; i < MIN2(mixer_devices->count, max_view); i++)
    {
      setHighlight(mixerscr, ok, i == current);
//      mvwaddstr(mixerscr, 4+i, 4, "/dev/");
//      waddstr(mixerscr, mixer_devices->name[top+i]);
      mvwaddstr(mixerscr, 4+i, 4, mixer_devices->name[top+i]);
    }
    wattroff(mixerscr, A_REVERSE);

    /*****
     * show up/down arrow to the left, if more items are available
     * than can be displayed
     *****/

    if (top > 0)
    {
      mvwaddch(mixerscr,4, 2, ACS_UARROW);
      mvwaddch(mixerscr,4+1, 2, ACS_VLINE);
    }
    if (mixer_devices->count > max_view && top+max_view <= mixer_devices->count-1)
    {
      mvwaddch(mixerscr,4+max_view-2, 2, ACS_VLINE);
      mvwaddch(mixerscr,4+max_view-1, 2, ACS_DARROW);
    }

    wmove(mixerscr, 1, 23);  /***** set cursor to an appropriate location */
    wrefresh (mixerscr);     /***** refresh the dialog */

    /* process the command keystroke */

    switch(getch())
    {
    case KEY_UP:   /***** cursor up */
      if (current > 0)
	current--;
      else
	top = (top > 0 ? top-1 : 0);
      break;
    case KEY_DOWN: /***** cursor down */
      if (top+current < mixer_devices->count-1)
      {
	if (current < max_view-1)
	 current++;
	else
	  top++;
      }
      break;
    case ESCAPE:  /***** press Escape to leave dialog */
      retval = MIXER_ERR;
      request_finish = 1;
      break;
    case ENTER:   /***** make selection with Enter or Space bar */
    case BLANK:
      setMixer(mixer_devices->name[top+current]); /***** set mixer device to highlighted item */
      retval = initMixer();                       /***** retval is ok, if initMixer() returned ok */
      request_finish = 1;                         /***** leave dialog */
      break;
    }
  }

  delwin(mixerscr);   /***** delete ncurses dialog window */
  return(retval);
}

/********************************************************************************
 * select audio from list of available audio devices
 ********************************************************************************/

int selectAudio()
{
  int top = 0, current = 0; /***** selection menu related variables */
  int max_view = 9;

  int retval = 0;           /***** return value */

  int request_finish = 0;   /***** leave current dialog, if request_finish is set to 1 */

  int width = 45, height = 15, i, j;               /***** ncurses related variables */
  WINDOW *audioscr = popupWindow (width, height);

  while (!request_finish)
  {
    wattrset (audioscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

    /*****
     * draw a box around the dialog window
     * and empty it.
     *****/
    werase (audioscr);
    box (audioscr, 0, 0);
    for (i = 1; i < width-1; i++)
      for (j = 1; j < height-1; j++)
	mvwaddch(audioscr, j, i, ' ');

    /***** dialog header */

    mvwaddstr(audioscr, 1, 2, "Select Audio Device:");
    mvwaddseparator(audioscr, 2, width);

    /***** selection area */

    for (i = 0; i < MIN2(audio_devices->count, max_view); i++)
    {
      setHighlight(audioscr, ok, i == current);
//      mvwaddstr(audioscr, 4+i, 4, "/dev/");
//      waddstr(audioscr, audio_devices->name[top+i]);
      mvwaddstr(audioscr, 4+i, 4, audio_devices->name[top+i]);
    }
    wattroff(audioscr, A_REVERSE);

    /*****
     * show up/down arrow to the left, if more items are available
     * than can be displayed
     *****/
    if (top > 0)
    {
      mvwaddch(audioscr,4, 2, ACS_UARROW);
      mvwaddch(audioscr,4+1, 2, ACS_VLINE);
    }
    if (audio_devices->count > max_view && top+max_view <= audio_devices->count-1)
    {
      mvwaddch(audioscr,4+max_view-2, 2, ACS_VLINE);
      mvwaddch(audioscr,4+max_view-1, 2, ACS_DARROW);
    }

    wmove(audioscr, 1, 23);  /***** set cursor to an appropriate location */
    wrefresh (audioscr);     /***** refresh the dialog */

    /* process the command keystroke */

    switch(getch())
    {
    case KEY_UP:   /***** cursor up */
      if (current > 0)
	current--;
      else
	top = (top > 0 ? top-1 : 0);
      break;
    case KEY_DOWN: /***** cursor down */
      if (top+current < audio_devices->count-1)
      {
	if (current < max_view-1)
	 current++;
	else
	  top++;
      }
      break;
    case ESCAPE:   /***** Hit Escape to leave dialog */
      retval = AUDIO_ERR;
      request_finish = 1;
      break;
    case ENTER:   /***** make selection with Enter or Space bar */
    case BLANK:
      setAudio(audio_devices->name[top+current]); /***** set audio device to highlighted item */
      retval = initAudio();                       /***** retval is ok, if initAudio() returned ok */
      if (retval == AUDIO_ERR)
	noAudio();
      request_finish = 1;                         /***** leave dialog */
      break;
    }
  }

  delwin(audioscr);   /***** delete ncurses dialog window */
  return(retval);
}

/********************************************************************************
 * Adjust Microphone/IGain Level
 ********************************************************************************/

int adjustMixerLevels()
{
  int mic_level    = 99; /***** initial values for Microphone and */

  int igain_level  = 0;  /***** Input Gain level */

  int max_sample   = 32500; /***** max 16-bit sample value coming from the sound card */

  int count;           /**** temporary variables */
  int max_gain;
  char tmp_string[10];

  int width = 45, height = 11, i, j;               /***** ncurses related variables */
  WINDOW *adjustscr = popupWindow (width, height);

  int retval = 0;  /***** return value */

  /* ***** display information */

  wattrset (adjustscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

  /*****
   * draw a box around the dialog window
   * and empty it.
   *****/
  werase (adjustscr);
  box (adjustscr, 0, 0);
  for (i = 1; i < width-1; i++)
    for (j = 1; j < height-1; j++)
      mvwaddch(adjustscr, j, i, ' ');

  setMicLevel(mic_level);

  if (mixerHasIGain() == MIXER_OK)
  {
    igain_level = 1;
    setIGainLevel(igain_level); /***** set initial levels in mixer */
  }
  else
    goto no_igain;

  /***** dialog header */

  mvwaddstr(adjustscr, 1, 2, "Adjust Input Gain Level:");
  mvwaddseparator(adjustscr, 2, width);

  /***** dialog message */

  mvwaddstr(adjustscr, 3, 2, "Please grab your microphone and speak");
  mvwaddstr(adjustscr, 4, 2, "nonsense at a conversational volume");
  mvwaddstr(adjustscr, 5, 2, "(speech recognition volume) until I");
  mvwaddstr(adjustscr, 6, 2, "say stop:");
  mvwaddstrcntr(adjustscr, 8, width, "Press any key to start ...");

  wmove(adjustscr, 1, 27);  /***** set cursor to an appropriate location */
  wrefresh (adjustscr);     /***** refresh the dialog */
  getch();                  /***** wait for keyboard reaction */

  /***** update dialog */

  mvwaddstr(adjustscr, 8, 2, "Current Gain Level:               ");
  sprintf(tmp_string, "%d", igain_level);
  mvwaddstr(adjustscr, 8, 22, tmp_string);

  wmove(adjustscr, 1, 27);  /***** set cursor to an appropriate location */
  wrefresh (adjustscr);     /***** refresh the dialog */

  count    = 0;
  max_gain = 0;

  if (openAudio() == AUDIO_ERR)
  {
    /*****
     * if the audio device could not be opened,
     * show a warning dialog and return to main menu
     *****/

    /***** empty dialog */

    for (i = 1; i < width-1; i++)
      mvwaddch(adjustscr, 1, i, ' ');
    for (i = 1; i < width-1; i++)
      for (j = 3; j < height-1; j++)
	mvwaddch(adjustscr, j, i, ' ');

    /***** show header and message */

    mvwaddstr(adjustscr, 1, 2, "Warning!");

    mvwaddstr(adjustscr, 5, 2, "Failed to open sound device!!");
    mvwaddstrcntr(adjustscr, 8, width, "Press any key to return to menu ...");
    wmove(adjustscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (adjustscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */

    retval = 0;               /***** set return value to ERROR */
    goto adjustMixerLevelsReturn;
  }

  /***** repeat until the input gain level has been adjusted properly */

  while (1)
  {
    int max;          /***** maximum sample value in a sequence of audio samples */
    int samples = 10; /***** number of blocks to get from the audio device */

    count++;

    /*****
     * get the maximum values of 'samples' blocks of data from the sound card
     * the maximum of these values is stored in 'max'9
     *****/
    max = 0;
    for (i = 0; i < samples; i++)
    {
      int value = getBlockMax(); /***** should check for value == -1 (case of an error!) */
      if (value > max)
	max = value;
    }

    /***** max_gain holds the highest maximum sample found */

    if (max > max_gain)
      max_gain = max;

    if (count == 5) /***** after five iterations, check the value of 'max_gain' */
    {
      /*****
       * if max_gain is too low (i.e. it is between silence and low_volume)
       * the input gain level is increased
       *****/
      if (max_gain >= silence * max_sample && max_gain < low_volume * max_sample)
      {
	igain_level++;
	if (igain_level > 99)
	  igain_level = 99;
	setIGainLevel(igain_level);

	sprintf(tmp_string, "%d", igain_level); /***** update display of igain level */
	mvwaddstr(adjustscr, 8, 22, tmp_string);

	wmove(adjustscr, 1, 27);  /***** set cursor to an appropriate location */
	wrefresh (adjustscr);     /***** refresh the dialog */
      }
      /*****
       * if the level is above 'low_volume'
       * we assume that the input gain is high enough and break the current while loop
       *****/
      else if (max_gain >= low_volume * max_sample)
	break;

      /***** reset count and max_gain */

      count    = 0;
      max_gain = 0;
    }
  }
  closeAudio(); /***** disconnect from microphone */

  /***** update dialog window */

  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(adjustscr, j, i, ' ');
  mvwaddstrcntr(adjustscr, 5, width, "STOP!!");
  mvwaddstrcntr(adjustscr, 8, width, "Press any key to continue ...");

  wmove(adjustscr, 1, 27);  /***** set cursor to an appropriate location */
  wrefresh (adjustscr);     /***** refresh the dialog */
  getch();                  /***** wait for keyboard reaction */

 no_igain:

  count = 0; /***** reset count */

  /***** adjusting microphone level */

  /***** dialog header */

  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(adjustscr, j, i, ' ');
  mvwaddstr(adjustscr, 1, 2, "Adjust Microphone Level:");

  /***** dialog message */

  mvwaddstr(adjustscr, 3, 2, "Please grab your microphone and speak");
  mvwaddstr(adjustscr, 4, 2, "very loudly (laughing loudly is good)");
  mvwaddstr(adjustscr, 5, 2, "until I say stop:");
  mvwaddstrcntr(adjustscr, 8, width, "Press any key to start ...");

  wmove(adjustscr, 1, 27);  /***** set cursor to an appropriate location */
  wrefresh (adjustscr);     /***** refresh the dialog */
  getch();                  /***** wait for keyboard reaction */

  /***** update dialog */

  mvwaddstr(adjustscr, 8, 2, "Current Microphone Level:               ");
  sprintf(tmp_string, "%d", mic_level);
  mvwaddstr(adjustscr, 8, 28, tmp_string);

  wmove(adjustscr, 1, 27);  /***** set cursor to an appropriate location */
  wrefresh (adjustscr);     /***** refresh the dialog */

  if (openAudio() == AUDIO_ERR)
  {
    /*****
     * if the audio device could not be opened,
     * show a warning dialog and return to main menu
     *****/

    /***** empty dialog */
    for (i = 1; i < width-1; i++)
      mvwaddch(adjustscr, 1, i, ' ');
    for (i = 1; i < width-1; i++)
      for (j = 3; j < height-1; j++)
	mvwaddch(adjustscr, j, i, ' ');

    /***** show header and message */

    mvwaddstr(adjustscr, 1, 2, "Warning!");

    mvwaddstr(adjustscr, 5, 2, "Failed to open sound device!!");
    mvwaddstrcntr(adjustscr, 8, width, "Press any key to return to menu ...");
    wmove(adjustscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (adjustscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */

    retval = 0;               /***** set return value to ERROR */
    goto adjustMixerLevelsReturn;
  }

  /***** repeat until the microphone level has been adjusted properly */

  while (1)
  {
    int max;         /***** maximum sample value in a sequence of audio samples */
    int samples = 4; /***** number of blocks to get from the audio device */

    /*****
     * get the maximum values of 'samples' blocks of data from the sound card
     * the maximum of these values is stored in 'max'9
     *****/
    max = 0;
    for (i = 0; i < samples; i++)
    {
      int value = getBlockMax(); /***** should check for value == -1 (case of an error!) */
      if (value > max)
	max = value;
    }

    if (max >= high_volume * max_sample)
    {
      /*****
       * if max is too high (i.e. above high_volume)
       * the microphone level is dereased
       *****/
      mic_level -= 1;
      if (mic_level < 5)
	mic_level = 5;
      setMicLevel(mic_level);

      count = 0; /***** reset count */

      sprintf(tmp_string, "%d", mic_level); /***** update display of mic level */
      mvwaddstr(adjustscr, 8, 28, tmp_string);

      wmove(adjustscr, 1, 27);  /***** set cursor to an appropriate location */
      wrefresh (adjustscr);     /***** refresh the dialog */
    }
    else if (max >= silence * max_sample)
    {
      /*****
       * if there is a signal above silence coming in,
       * increase count
       * if count >= a specified constant, we assume that
       * the microphone level is not too high any more and
       * thus, break the while loop
       ****/
      count++;

      if (count >= 20)
	break;
    }
    else
    {
      /*****
       * this was a silence frame, decrease count if it is > 0
       *****/
      if (count > 0)
	count--;
    }
  }
  closeAudio(); /***** disconnect from microphone */

  /***** clear dialog window */

  for (i = 1; i < width-1; i++)
    mvwaddch(adjustscr, 1, i, ' ');
  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(adjustscr, j, i, ' ');

  /***** check the mixer values, to make sure they look reasonable */

  if (igain_level >= mic_level ||
      mic_level < MIN_REASONABLE_MIC_LEVEL ||
      igain_level > MAX_REASONABLE_IGAIN_LEVEL)
  {
    int pos = 0;

    /***** dialog header */

    mvwaddstr(adjustscr, 1, 2, "Warning!");

    /***** dialog message */

    mvwaddstr(adjustscr, 3, 2, "You have to run this step once again!");
    mvwaddstr(adjustscr, 4, 2, "The estimated level results don't look");
    mvwaddstr(adjustscr, 5, 2, "reasonable to me!");
    if (igain_level >= mic_level || igain_level > MAX_REASONABLE_IGAIN_LEVEL)
      mvwaddstr(adjustscr, 6+(pos++), 2, "- Input gain level looks too high!");
    if (igain_level >= mic_level || mic_level < MIN_REASONABLE_MIC_LEVEL)
      mvwaddstr(adjustscr, 6+(pos++), 2, "- Microphone level looks too low?!");
    mvwaddstrcntr(adjustscr, 8, width, "Press any key to return to menu ...");

    wmove(adjustscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (adjustscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */

    retval = 0;
  }
  else /***** the mixer values seem ok */
  {
    /***** dialog header */

    mvwaddstr(adjustscr, 1, 2, "Success!");

    /***** dialog message */

    mvwaddstrcntr(adjustscr, 4, width, "The mixer levels have been");
    mvwaddstrcntr(adjustscr, 5, width, "adjusted successfully!");
    mvwaddstrcntr(adjustscr, 8, width, "Press any key to return to menu ...");

    wmove(adjustscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (adjustscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */

    retval = 1; /***** set return value = ok */
  }

 adjustMixerLevelsReturn:
  delwin(adjustscr);   /***** delete ncurses dialog window */
  return(retval);
}

/********************************************************************************
 * calculate recording thresholds
 ********************************************************************************/

int calculateThresholds()
{
  int samples;
  int max = 0, value;
  int silence_level_tmp; /***** need int (instead of short) to sum up several values */
  int silence_max;

  int retval = 0;  /***** return value */

  int width = 45, height = 11, i, j;               /***** ncurses related variables */
  WINDOW *threshscr = popupWindow (width, height);

  /*****
   * first step: define silence level
   *****/

  wattrset (threshscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

  /*****
   * draw a box around the dialog window
   * and empty it.
   *****/
  werase (threshscr);
  box (threshscr, 0, 0);
  for (i = 1; i < width-1; i++)
    for (j = 1; j < height-1; j++)
      mvwaddch(threshscr, j, i, ' ');

  /***** dialog header */

  mvwaddstr(threshscr, 1, 2, "Calculate Thresholds:");
  mvwaddseparator(threshscr, 2, width);

  /***** dialog message */

  mvwaddstr(threshscr, 3, 2, "I'll calculate some values now. First,");
  mvwaddstr(threshscr, 4, 2, "I need to know the silence level of the");
  mvwaddstr(threshscr, 5, 2, "microphone. Please be silent until I");
  mvwaddstr(threshscr, 6, 2, "say I'm done:");
  mvwaddstrcntr(threshscr, 8, width, "Press any key to start ...");

  wmove(threshscr, 1, 24);  /***** set cursor to an appropriate location */
  wrefresh (threshscr);     /***** refresh the dialog */
  getch();                  /***** wait for keyboard reaction */

  /***** clear dialog window */

  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(threshscr, j, i, ' ');

  mvwaddstrcntr(threshscr, 5, width, "stand by ..."); /***** update dialog */
  wmove(threshscr, 1, 24);  /***** set cursor to an appropriate location */
  wrefresh (threshscr);     /***** refresh the dialog */

  if (openAudio() == AUDIO_ERR)
  {
    /*****
     * if the audio device could not be opened,
     * show a warning dialog and return to main menu
     *****/

    /***** clear dialog */

    for (i = 1; i < width-1; i++)
      mvwaddch(threshscr, 1, i, ' ');
    for (i = 1; i < width-1; i++)
      for (j = 3; j < height-1; j++)
	mvwaddch(threshscr, j, i, ' ');

    /***** show header and message */

    mvwaddstr(threshscr, 1, 2, "Warning!");

    mvwaddstr(threshscr, 5, 2, "Failed to open sound device!!");
    mvwaddstrcntr(threshscr, 8, width, "Press any key to return to menu ...");
    wmove(threshscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (threshscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */

    retval = 0;               /***** set return value to ERROR */
    goto calculateThresholdsReturn;
  }

  /*****
   * define the silence_level as the average of a specified number
   * of subsequent block maxima
   *****/
  samples           = 40;
  silence_level_tmp = 0;
  silence_max   = 0;
  for (i = 0; i < samples; i++)
  {
    int value = getBlockMax(); /***** should check for value == -1 (case of an error!) */
      silence_level_tmp += value;
    if (value > silence_max)
      silence_max = value;
  }
  silence_level_tmp /= samples;
  silence_level = silence_level_tmp;

  /***** rec_level and stop_level */

  /***** clear dialog */

  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(threshscr, j, i, ' ');

  /***** update dialog */

  mvwaddstr(threshscr, 3, 2, "Now, I'll define the thresholds at which");
  mvwaddstr(threshscr, 4, 2, "to start/stop recording. Please talk");
  mvwaddstr(threshscr, 5, 2, "at a conversational volume (speech");
  mvwaddstr(threshscr, 6, 2, "recognition volume) until I say stop!");
  mvwaddstrcntr(threshscr, 8, width, "Press any key to start ...");

  wmove(threshscr, 1, 24);  /***** set cursor to an appropriate location */
  wrefresh (threshscr);     /***** refresh the dialog */
  getch();                  /***** wait for keyboard reaction */

  /***** clear dialog */

  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(threshscr, j, i, ' ');

  mvwaddstrcntr(threshscr, 5, width, "keep on talking ..."); /***** update dialog */
  wmove(threshscr, 1, 24);  /***** set cursor to an appropriate location */
  wrefresh (threshscr);     /***** refresh the dialog */

  /***** get maximum value 'max' of a prespecified number of subsequent block maxima */

  samples = 80;
  for (i = 0; i < samples; i++)
  {
    value = getBlockMax(); /***** should check for value == -1 (case of an error!) */
    if (value > max)
      max = value;
  }

  /***** set the rec_level  to be the average of silence_level and 'max' */
  /***** set the stop_level to be three quarters of silence_level plus one quarter of 'max' */

  rec_level =  (silence_level + max) / 2;
  stop_level = (3*silence_level + max) / 4;

  closeAudio(); /***** disconnect from microphone */

  /***** clear dialog */

  for (i = 1; i < width-1; i++)
    mvwaddch(threshscr, 1, i, ' ');
  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(threshscr, j, i, ' ');

  /***** check that the thresholds are reasonable */

  if (silence_level >= stop_level ||
      silence_max >= stop_level    ||
      silence_level < 0 || stop_level < 0 || rec_level < 0)
  {
    /***** dialog header */

    mvwaddstr(threshscr, 1, 2, "Warning!");

    /***** dialog message */

    mvwaddstr(threshscr, 4, 2, "You have to run this step once again!");
    mvwaddstr(threshscr, 5, 2, "The calculated thresholds don't look");
    mvwaddstr(threshscr, 6, 2, "reasonable to me!");
    mvwaddstrcntr(threshscr, 8, width, "Press any key to return to menu ...");
    wmove(threshscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (threshscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */

    retval = 0;
  }
  else /***** the values seem ok */
  {
    /***** dialog header */

    mvwaddstr(threshscr, 1, 2, "Success!");

    /***** dialog message */

    mvwaddstr(threshscr, 4, 2, "You may stop talking now! The thresholds");
    mvwaddstr(threshscr, 5, 2, "have been defined!");

    /* Debugging stuff:
       mvwaddint(threshscr, 6, 2, silence_level);
       mvwaddint(threshscr, 6,10, stop_level);
       mvwaddint(threshscr, 6,20, rec_level);
       */

    mvwaddstrcntr(threshscr, 8, width, "Press any key to return to menu ...");

    wmove(threshscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (threshscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */

    retval = 1; /***** set return value = ok */
  }

 calculateThresholdsReturn:
  delwin(threshscr);   /***** delete ncurses dialog window */
  return(retval);
}

/********************************************************************************
 * estimate channel mean
 ********************************************************************************/

int estimateChannelMean()
{
  int result;
  int retval = 0;  /***** return value */

  int width = 45, height = 11, i, j;               /***** ncurses related variables */
  WINDOW *channelscr = popupWindow (width, height);

  wattrset (channelscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

  /*****
   * draw a box around the dialog window
   * and empty it.
   *****/
  werase (channelscr);
  box (channelscr, 0, 0);
  for (i = 1; i < width-1; i++)
    for (j = 1; j < height-1; j++)
      mvwaddch(channelscr, j, i, ' ');

  /***** dialog header */

  mvwaddstr(channelscr, 1, 2, "Estimating Channel Characteristics:");
  mvwaddseparator(channelscr, 2, width);

  /***** dialog message */

  mvwaddstr(channelscr, 4, 2, "I'll calculate the characteristics of");
  mvwaddstr(channelscr, 5, 2, "the recording channel now. Please remain");
  mvwaddstr(channelscr, 6, 2, "silent until I say I'm done:");
  mvwaddstrcntr(channelscr, 8, width, "Press any key to start ...");
  wmove(channelscr, 1, 38);  /***** set cursor to an appropriate location */
  wrefresh (channelscr);     /***** refresh the dialog */
  getch();                  /***** wait for keyboard reaction */

  /***** clear dialog */

  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(channelscr, j, i, ' ');

  /***** update dialog */

  mvwaddstrcntr(channelscr, 5, width, "stand by ...");
  wmove(channelscr, 1, 38);  /***** set cursor to an appropriate location */
  wrefresh (channelscr);     /***** refresh the dialog */

  if (openAudio() == AUDIO_ERR)
  {
    /*****
     * if the audio device could not be opened,
     * show a warning dialog and return to main menu
     *****/

    /***** clear dialog */

    for (i = 1; i < width-1; i++)
      mvwaddch(channelscr, 1, i, ' ');
    for (i = 1; i < width-1; i++)
      for (j = 3; j < height-1; j++)
	mvwaddch(channelscr, j, i, ' ');

    /***** show header and message */

    mvwaddstr(channelscr, 1, 2, "Warning!");

    mvwaddstr(channelscr, 5, 2, "Failed to open sound device!!");
    mvwaddstrcntr(channelscr, 8, width, "Press any key to return to menu ...");
    wmove(channelscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (channelscr);     /***** refresh the dialog */
    getch();                   /***** wait for keyboard reaction */

    retval = 0;                /***** set return value to ERROR */
    goto estimateChannelMeanReturn;
  }

  result = calculateChannelMean(); /***** calculate the characteristics of the recording channel */

  closeAudio(); /***** disconnect from the microphone */

  /***** clear dialog */

  for (i = 1; i < width-1; i++)
    mvwaddch(channelscr, 1, i, ' ');
  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(channelscr, j, i, ' ');

  if (result == AUDIO_ERR)
  {
    /***** if an error occurred during the calculation of the channel mean ... */

    retval = 0; /***** set return value to ERROR */

    /***** dialog header and message */

    mvwaddstr(channelscr, 1, 2, "Error!");

    mvwaddstr(channelscr, 3, 2, "I didn't manage to estimate the channel");
    mvwaddstr(channelscr, 4, 2, "characteristics for some unknown reason!");
    mvwaddstr(channelscr, 5, 2, "Make sure that no application uses the");
    mvwaddstr(channelscr, 6, 2, "sound card and then try again!");
    mvwaddstrcntr(channelscr, 8, width, "Press any key to return to menu ...");

    wmove(channelscr, 1, 9);  /***** set cursor to an appropriate location */
    wrefresh (channelscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */
  }
  else /***** channel mean calculation was ok */
  {
    retval = 1; /***** set return value to ok */

    /***** dialog header and message */

    mvwaddstr(channelscr, 1, 2, "Success!");

    mvwaddstr(channelscr, 4, 2, "You may stop talking now! The channel");
    mvwaddstr(channelscr, 5, 2, "characteristics have been estimated!");
    mvwaddstrcntr(channelscr, 8, width, "Press any key to return to menu ...");

    wmove(channelscr, 1, 11);  /***** set cursor to an appropriate location */
    wrefresh (channelscr);     /***** refresh the dialog */
    getch();                  /***** wait for keyboard reaction */
  }

 estimateChannelMeanReturn:
  delwin(channelscr);   /***** delete ncurses dialog window */
  return(retval);
}

/********************************************************************************
 * save configuration
 ********************************************************************************/

int saveConfiguration()
{
    int retval = 0;  /***** return value */

    int width = 60, height = 11, i, j;               /***** ncurses related variables */
    WINDOW *savescr = popupWindow (width, height);

    wattrset (savescr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

  /*****
   * draw a box around the dialog window
   * and empty it.
   *****/
    werase (savescr);
    box (savescr, 0, 0);
    for (i = 1; i < width-1; i++)
	for (j = 1; j < height-1; j++)
	    mvwaddch(savescr, j, i, ' ');

  /***** dialog header */

  mvwaddstr(savescr, 1, 2, "Save Configuration:");
  mvwaddseparator(savescr, 2, width);

  /***** dialog message */

  mvwaddstr(savescr, 4, 2, "Your configuration will be saved to");
  mvwaddstrcntr(savescr, 6, width, CONFIG_DIR"/config" );
  mvwaddstrcntr(savescr, 8, width, "Press any key to proceed ...");

  wmove(savescr, 1, 22);  /***** set cursor to an appropriate location */
  wrefresh (savescr);     /***** refresh the dialog */
  getch();                  /***** wait for keyboard reaction */

    /***** clear dialog */

    for (i = 1; i < width-1; i++)
	for (j = 3; j < height-1; j++)
	    mvwaddch(savescr, j, i, ' ');

    wmove(savescr, 1, 22);  /***** set cursor to an appropriate location */
    wrefresh (savescr);     /***** refresh the dialog */

    /***** config directory */
    char * config_dir = CONFIG_DIR;
    if( config_dir[0] == '~' )
    {
	const char * home = getenv( "HOME" );
	if( !home ) home = "/tmp";
	config_dir++;
	char *dir = alloca( strlen( home ) + strlen( config_dir ) + 1 );
	sprintf( dir, "%s%s", home, config_dir );
	config_dir = dir;
    }

    if( mkpath( config_dir, 0700 ) )
    {
	mvwaddstr(savescr, 5, 2, "Failed to create config directory! Oops!");
	mvwaddstr(savescr, 6, 2, "What's going on?");
	mvwaddstrcntr(savescr, 8, width, "Press any key to return to menu ...");

	wmove(savescr, 1, 22);  /***** set cursor to an appropriate location */
	wrefresh (savescr);     /***** refresh the dialog */
	getch();                /***** wait for keyboard reaction */

	retval = 0;  /***** set return value to ERROR */
	goto saveConfigurationReturn;
    }

  /***** config_file = config_dir+"config" */

  char * config_file = alloca( strlen(config_dir) + strlen("/config") + 1 );
  strcpy(config_file, config_dir);
  strcat(config_file, "/config");

    FILE *f = fopen( config_file, "w" );

  if( !f ) /***** failed to write config file */
  {
    /***** dialog message */

    mvwaddstr(savescr, 5, 2, "Failed to create your configuration file! Oops!");
    mvwaddstr(savescr, 6, 2, "What's going on?");
    mvwaddstrcntr(savescr, 8, width, "Press any key to return to menu ...");

    wmove(savescr, 1, 22);  /***** set cursor to an appropriate location */
    wrefresh (savescr);     /***** refresh the dialog */
    getch();                /***** wait for keyboard reaction */

    retval = 0;  /***** set return value to ERROR */
    goto saveConfigurationReturn;
  }

  /***** output configuration information to config file */

  fprintf(f, "Mixer Device    = %s\n", getMixer());
  fprintf(f, "Audio Device    = %s\n", getAudio());
  fprintf(f, "Mic Level       = %d\n", mic_level);
  fprintf(f, "IGain Level     = %d\n", igain_level);
  fprintf(f, "Record Level    = %d\n", rec_level);
  fprintf(f, "Stop Level      = %d\n", stop_level);
  fprintf(f, "Silence Level   = %d\n", silence_level);
  fprintf(f, "Channel Mean    =");
  for (i = 0; i < FEAT_VEC_SIZE; i++)
    fprintf(f, " %6.5f", channel_mean[i]);
  fprintf(f, "\n");
  fclose(f);

  /***** clear dialog */

  for (i = 1; i < width-1; i++)
    mvwaddch(savescr, 1, i, ' ');
  for (i = 1; i < width-1; i++)
    for (j = 3; j < height-1; j++)
      mvwaddch(savescr, j, i, ' ');

  /***** update dialog to tell user that the configuration has been saved successfully */

  mvwaddstr(savescr, 1, 2, "Success!");

  mvwaddstr(savescr, 4, 2, "Your configuration has been saved successfully to");
  mvwaddstr(savescr, 5, 4, config_file);
  mvwaddstr(savescr, 7, 2, "CVoiceControl is now ready to use!");
  mvwaddstrcntr(savescr, 9, width, "Press any key to return to menu ...");

  retval = 1; /***** set return value to ok */

  wmove(savescr, 1, 11);  /***** set cursor to an appropriate location */
  wrefresh (savescr);     /***** refresh the dialog */
  getch();                /***** wait for keyboard reaction */

 saveConfigurationReturn:

  return(retval);
}


/********************************************************************************
 * main
 ********************************************************************************/

int main(int argc, char *argv[])
{
  /*****
   * boolean value indicates when to break main loop
   * (and thus finish this configuration tool)
   *****/
  int request_finish = 0;

  int current         = 0; /***** menu related variables */
  int menu_items      = 7;
  enum state status[] = {invalid, invalid, inactive, inactive, inactive, inactive, invalid};

  WINDOW *mainscr; /***** ncurses related variables */
  int i, j;

  /* ***** detect available mixer devices */

  mixer_devices = scanMixerDevices();
  if (mixer_devices == NULL || mixer_devices->count == 0)
  {
    /* ***** no mixer devices available -> exit! */

    fprintf(stderr, "No mixer devices available!\n");
    fprintf(stderr, "Please purchase a sound card and install it!\n");
    exit(-1);
  }
  else
  {
    if (mixer_devices->count == 1) /***** exactly one mixer device available */
    {
      setMixer(mixer_devices->name[0]); /***** set this mixer */

      if (initMixer() == MIXER_OK) /***** if mixer is ok, keep it */
      {
	status[0] = ok;
	status[2] = invalid;
      }
      else                         /***** otherwise, exit!*/
      {
	fprintf(stderr, "Couldn't init the only available mixer device: %s\n",
		mixer_devices->name[0]);
	exit(-1);
      }
    }
    else /* ***** more than one device available! */
    {
      /* ***** use /dev/mixer as default if it exists */

      for (i = 0; i < mixer_devices->count; i++)
      {
	if (strcmp(mixer_devices->name[i], "mixer") == 0)
	{
	  setMixer("mixer");
	  if (initMixer() == MIXER_OK)
	  {
	    status[0] = ok;
	    status[2] = invalid;
	  }
	  else
	    noMixer();

	  break;
	}
      }
    }
  }

  /* ***** detect available audio devices */

  audio_devices = scanAudioDevices();
  if (audio_devices == NULL || audio_devices->count == 0)
  {
    /* ***** no audio devices available! */

    fprintf(stderr, "No audio device available that\n");
    fprintf(stderr, "supports 16bit recording!\n");
    fprintf(stderr, "Please purchase a sound card and install it!\n");
    exit(-1);
  }
  else
  {
    if (audio_devices->count == 1) /***** exactly one audio device available */
    {
      setAudio(audio_devices->name[0]); /***** set this audio device */

      if (initAudio() == AUDIO_OK) /***** if audio device is ok, keep it */
      {
	status[1] = ok;
      }
      else                         /***** otherwise, exit!*/
      {
	fprintf(stderr, "Couldn't init the only available audio device: %s\n",
		audio_devices->name[0]);
	exit(-1);
      }
    }
    else /* ***** more than one device available! */
    {
      /* ***** use /dev/dspW as default if it exists */

      for (i = 0; i < audio_devices->count; i++)
      {
	if (strcmp(audio_devices->name[i], "dspW") == 0)
	{
	  setAudio("dspW");
	  if (initAudio() == AUDIO_OK)
	    status[1] = ok;
	  else
	    noAudio();

	  break;
	}
      }
    }
  }

  /*****
   * if mixer and audio device have been selected successfully,
   * set menu cursor to next available menu item
   *****/
  if (status[0] == ok && status[1] == ok)
    current = 2;

  /***** ignore Ctrl-C */

  signal(SIGINT, SIG_IGN);

  /* ***** ncurses stuff */

  initscr();      /* initialize the curses library */

  if (color_term != -1) /***** define dialog color pairs if terminal supports colors */
   {
      start_color ();
      if ((color_term = has_colors ()))
      {
         color_term = 1;
         init_pair (1, COLOR_WHITE, COLOR_BLUE);
         init_pair (2, COLOR_YELLOW, COLOR_BLUE);
         init_pair (3, COLOR_BLUE, COLOR_YELLOW);
         init_pair (4, COLOR_YELLOW, COLOR_CYAN);
      }
   }
   else
      color_term = 0;

  keypad(stdscr, TRUE);  /* enable keyboard mapping */
  scrollok (stdscr, FALSE);
  cbreak();              /* take input chars one at a time, no wait for \n */
  noecho();              /* don't echo input */
  refresh();

  mainscr = popupWindow(COLS, LINES); /***** dialog window that contains the main menu */
  leaveok (mainscr, FALSE);

  while (!request_finish)
  {
    wattrset (mainscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

    /*****
     * draw a box around the dialog window
     * and empty it.
     *****/
    box(mainscr, 0, 0);
    for (i = 1; i < COLS-1; i++)
      for (j = 1; j < LINES-1; j++)
	mvwaddch(mainscr, j, i, ' ');

    /***** dialog header */

    mvwaddstr(mainscr, 1, 2, "CVoiceControl");
    mvwaddstr(mainscr, 1, COLS - strlen("(c) 2000 Daniel Kiecza") - 2, "(c) 2000 Daniel Kiecza");
    mvwaddseparator(mainscr, 2, COLS);

    mvwaddstr(mainscr, 3, (COLS / 2) - (strlen ("Recording Device Configuration Tool") / 2),
	      "Recording Device Configuration Tool");
    mvwaddseparator(mainscr, 4, COLS);

    /***** main menu */

    mvwaddstr(mainscr, 5, 2, "Please Select:");

    setHighlight(mainscr, status[0], current == 0);
    mvwaddstr(mainscr, 7,5,"Select Mixer Device");
    if (mixerOK() == MIXER_OK)
    {
      mvwaddstr(mainscr, 7,24," ("); waddstr(mainscr, getMixer()); waddstr(mainscr, ")");
    }
    else
      mvwaddstr(mainscr, 7,24," (none selected!)");

    setHighlight(mainscr, status[1], current == 1);
    mvwaddstr(mainscr, 8,5,"Select Audio Device");
    if (audioOK() == AUDIO_OK)
    {
      mvwaddstr(mainscr, 8,24," ("); waddstr(mainscr, getAudio()); waddstr(mainscr, ")");
    }
    else
      mvwaddstr(mainscr, 8,24," (none selected!)");

    setHighlight(mainscr, status[2], current == 2);
    mvwaddstr(mainscr,  9,5,"Adjust Mixer Levels");
    setHighlight(mainscr, status[3], current == 3);
    mvwaddstr(mainscr, 10,5,"Calculate Recording Thresholds");
    setHighlight(mainscr, status[4], current == 4);
    mvwaddstr(mainscr, 11,5,"Estimate Characteristics of Recording Channel");
    setHighlight(mainscr, status[5], current == 5);
    mvwaddstr(mainscr, 12,5,"Write Configuration");
    setHighlight(mainscr, status[6], current == 6);
    mvwaddstr(mainscr, 13,5,"Exit");

    wmove(mainscr, 5, 17);  /***** set cursor to an appropriate location */
    wrefresh(mainscr);     /***** refresh the dialog */

    /* process the command keystroke */

    switch(getch())
    {
    case KEY_UP:   /***** cursor up */
      current = (current == 0 ? menu_items - 1 : current - 1);
      while(status[current] == inactive)
	current = (current == 0 ? menu_items - 1 : current - 1);
      break;
    case KEY_DOWN: /***** cursor down */
      current = (current == menu_items-1 ? 0 : current + 1);
      while(status[current] == inactive)
	current = (current == menu_items-1 ? 0 : current + 1);
      break;
    case ENTER:    /***** handle menu selections */
    case BLANK:
      switch (current)
      {
      case 0: /***** select mixer device */
	status[0] = invalid;
	status[2] = inactive;
	status[3] = inactive;
	status[4] = inactive;
	status[5] = inactive;
	noMixer();
	if (selectMixer() == MIXER_OK)
	{
	  status[0] = ok;
	  status[2] = invalid;
	}
	break;
      case 1: /***** select audio device */
	status[1] = invalid;
	status[3] = inactive;
	status[4] = inactive;
	status[5] = inactive;
	noAudio();
	if (selectAudio() == AUDIO_OK)
	  status[1] = ok;
	break;
      case 2: /***** adjust mixer levels */
	if (adjustMixerLevels())
	{
	  status[2] = ok;
	  status[3] = invalid;
	  status[4] = invalid;
	}
	break;
      case 3: /***** calculate recording thresholds */
	if (calculateThresholds())
	  status[3] = ok;
	else
	  status[3] = invalid;
	break;
      case 4: /***** estimate the characteristics of the recording channel */
	if (estimateChannelMean())
	  status[4] = ok;
	else
	  status[4] = invalid;
	break;
      case 5: /***** save configuration! */
	if (saveConfiguration())
	{
	  status[5] = ok;
	  status[6] = ok;
	}
	break;
      case 6: /***** leave program */
	if (status[6] == ok  ||  (status[6] != ok && safeExit()))
	{
	  wrefresh(mainscr);     /***** refresh the dialog */
  	  request_finish = 1;
	  delwin(mainscr);   /***** delete ncurses dialog window */
	 }
	break;
      }
      break;
    }

    /***** if the configuration is done, activate the menu item "Save Configuration" */

    if (status[0] != ok || status[1] != ok ||
	status[2] != ok || status[3] != ok ||
	status[4] != ok)
      status[5] = inactive;
    else if (status[5] != ok)
      status[5] = invalid;
  }

  endwin();               /* we're done */

  /***** free memory used by the list of mixer and audio devices */

  if (mixer_devices != NULL)
  {
    for (i = 0; i < mixer_devices->count; i++)
      free(mixer_devices->name[i]);
    free(mixer_devices->name);
    free(mixer_devices);
  }

  if (audio_devices != NULL)
  {
    for (i = 0; i < audio_devices->count; i++)
      free(audio_devices->name[i]);
    free(audio_devices->name);
    free(audio_devices);
  }

  exit(0);
}
