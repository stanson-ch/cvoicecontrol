/***************************************************************************
                          model_editor.c  -  tool to edit speaker models
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
#include <time.h>

#include "model_editor.h"

#include "model.h"
#include "mixer.h"
#include "audio.h"
#include "preprocess.h"
#include "configuration.h"

#include "ncurses_tools.h"

Model *model;                  /***** speaker model */
char *model_file_name = NULL;  /***** name of speaker model file */
int modified = 0;              /***** tells whether the model has been modified */

/********************************************************************************
 * used to highlight an item that is currently selected from a list of menu items
 ********************************************************************************/

void setHighlight(WINDOW *window, enum state s, bool current)
{
  wattroff(window, A_INVIS|A_NORMAL|A_BOLD);

  /***** current item is displayed in reverse mode */

  if (current)
      wattron(window, A_REVERSE);
  else
      wattroff(window, A_REVERSE);

  /***** if an item is inactive, do not display it! */

  if (s == inactive)
    wattron(window, A_INVIS);
  else if (s == active)
    wattron(window, A_BOLD);
}

/********************************************************************************
 * make sure a modified speaker model is saved before the data is lost
 ********************************************************************************/

int safeResetModel()
{
  int retval = 0; /***** return value */

  /*****
   * if speaker model has been modified,
   * display a "warning" dialog to avoid an accidental loss of data
   *****/

  if (modified)
  {
    int width = 45, height = 11, i, j;               /***** ncurses related variables */
    WINDOW *resetscr = popupWindow (width, height);

    wattrset (resetscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

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

    mvwaddstr(resetscr, 4, 2,"You did not save the current model!");
    mvwaddstr(resetscr, 5, 2,"Press 'y' to proceed.  **Caution**");
    mvwaddstr(resetscr, 6, 2,"All data in the model will be lost!!");
    mvwaddstr(resetscr, 7, 2,"Press any other key to return to the");
    mvwaddstr(resetscr, 8, 2,"menu ...");

    wmove(resetscr, 1, 11); /***** set cursor to a convenient location */
    wrefresh (resetscr);    /***** and display dialog */

    /***** wait for keyboard input */

    if (getch() == 'y') /***** if user hits 'y' */
    {
      modified = 0;     /***** we reset the speaker model */
      resetModel(model);
      if (model_file_name != NULL) /***** if a file name had been specified, ... */
        free(model_file_name);     /***** free any memory used by it */
      model_file_name = NULL;
      retval = 1;                  /***** return ok */
    }
    else                /***** any other key -> keep speaker model */
      retval = 0;

    delwin(resetscr);   /***** delete ncurses dialog window */
  }
  else
    retval = 1; /***** if speaker model hasn't been modified, return ok */

  return(retval);
}

/********************************************************************************
 * play a sample's wave
 ********************************************************************************/

int playSample(ModelItemSample *sample)
{
  /***** play sample->wav_data */

  if (initAudio() == AUDIO_ERR) /***** make sure the audio device is initialized properly */
    return AUDIO_ERR;

  /***** call playUtterance in audio.c to actually play the utterance */

  return (playUtterance(sample->wav_data, sample->wav_length));
}

/********************************************************************************
 * record a sample utterance
 ********************************************************************************/

ModelItemSample *recordSample()
{
  /***** allocate memory for the new sample */
  ModelItemSample *new_sample = (ModelItemSample *)malloc(sizeof(ModelItemSample));

  char *tmp_string;
  time_t timer;     /***** used to get the current time, which will become */
  time(&timer);     /***** the main part of the new utterance's ID */

  /***** initialize the audio device */

  if (initMixer() == MIXER_ERR) /***** if mixer error, return nothing */
  {
    free(new_sample);
    return NULL;
  }
  if (igain_level > 0)
    setIGainLevel(igain_level); /***** set IGain and Mic level according */
  setMicLevel(mic_level);     /***** to configuration */

  if (initAudio() == AUDIO_ERR) /***** if audio error, return nothing */
  {
    free(new_sample);
    return NULL;
  }

  /***** connect to microphone, get utterance, disconnect */

  openAudio();
  new_sample->wav_data = getUtterance(&new_sample->wav_length);
  closeAudio();


  if (new_sample->wav_data == NULL) /***** if nothing was recorded, return nothing */
  {
    new_sample->wav_length = 0;
    new_sample->has_wav    = 0;
    free(new_sample);
    return NULL;
  }
  else
    /***** flag says that this sample utterance also contains its original
     * wave data, not only the preprocessed feature vectors
     *****/
    new_sample->has_wav    = 1;

  /***** preprocess the wave data */

  new_sample->data = preprocessUtterance(new_sample->wav_data, new_sample->wav_length, &new_sample->length);

  if (new_sample->data == NULL) /***** if preprocessing failed, return nothing */
  {
    new_sample->length = 0;
    free(new_sample->wav_data);
    free(new_sample);
    return NULL;
  }

  /***** set ID */

  tmp_string = ctime(&timer); /***** get current time */

  /***** set sample ID looks like:  [Thu Feb 10 12:10:53 2000] */

  new_sample->id = malloc( strlen( tmp_string ) + 3 );
  sprintf( new_sample->id, "[%s]", tmp_string );

  new_sample->next   = NULL; /***** next sample pointer is NULL */
  {
    int i;
    for (i = 0; i < 3; i++)
      new_sample->matrix[i] = NULL;
  }

  modified = 1; /***** speaker model has been modified now */

  return(new_sample);
}

/********************************************************************************
 * edit speaker model item
 ********************************************************************************/

void editModelItem(ModelItem *model_item)
{
  int width = 65, height = 22, i, j;             /***** ncurses related variables */
  WINDOW *itemscr = popupWindow (width, height);

  int top = 0, current = 0;   /***** menu selection related variables */
  int max_view = 6;

  int input_field_width = 40; /***** width of string input field */

  int request_finish = 0;     /***** request_finish = 1 breaks the main while loop */

  while (!request_finish)
  {
    wattrset (itemscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL);  /***** set bg color of the dialog */

    /*****
     * draw a box around the dialog window
     * and empty it.
     *****/
    werase (itemscr);
    box (itemscr, 0, 0);
    for (i = 1; i < width-1; i++)
      for (j = 1; j < height-1; j++)
	mvwaddch(itemscr, j, i, ' ');

    /***** dialog header */

    mvwaddstr(itemscr, 1, 2, "Edit Speaker Model Item");
    mvwaddseparator(itemscr, 2, width);

    /***** display help at the bottom */

    mvwaddseparator(itemscr, height-4, width);
    mvwaddstr(itemscr, height-3, 2, "r = record sample, d = delete sample, enter = play sample");
    mvwaddstr(itemscr, height-2, 2, "b = back to model menu, l = edit label, c = edit command");

    /***** display information about current speaker item */

    mvwaddstr(itemscr, 4, 2, "Label   :");
    mvwaddnstr(itemscr, 4, 12, model_item->label, input_field_width);
    mvwaddstr(itemscr, 5, 2, "Command :");
    mvwaddnstr(itemscr, 5, 12, model_item->command, input_field_width);

    mvwaddstr(itemscr, 7, 2, "Number of samples:");
    mvwaddint(itemscr, 7, 21, model_item->number_of_samples);

    /***** show sample utterances in a list. */

    mvwaddstr(itemscr, 9, 4, "ID"); /***** header */
    for (i = 0; i < 28; i++)
      mvwaddch(itemscr,10, 3+i, ACS_HLINE);

    /***** list items */

    for (i = 0; i < MIN2(model_item->number_of_samples, max_view); i++)
    {
      setHighlight(itemscr, active, i == current);
      mvwaddstr(itemscr,11+i,  3, "                            ");
      mvwaddstr(itemscr,11+i,  4, getModelItemSample(model_item, top+i)->id);
    }
    wattroff(itemscr, A_REVERSE);

    /***** up/down arrows indicate that not all items can be displayed */

    if (top > 0)
    {
      mvwaddch(itemscr,11, 2, ACS_UARROW);
      mvwaddch(itemscr,11+1, 2, ACS_VLINE);
    }
    if (model_item->number_of_samples > max_view && top+max_view <= model_item->number_of_samples-1)
    {
      mvwaddch(itemscr,11+max_view-2, 2, ACS_VLINE);
      mvwaddch(itemscr,11+max_view-1, 2, ACS_DARROW);
    }

    wmove(itemscr, 1,26); /***** move cursor to a convenient location */
    wrefresh(itemscr);    /***** refresh dialog window */

    /* process the command keystroke */

    switch(getch())
    {
    case KEY_UP: /***** cursor up */
      if (top+current > 0)
      {
	if (current > 0)
	 current--;
	else
	  top--;
      }
      break;
    case KEY_DOWN: /***** cursor down */
      if (top+current < model_item->number_of_samples-1)
      {
	if (current < max_view-1)
	 current++;
	else
	  top++;
      }
      break;
    case 'b':      /***** leave menu */
      request_finish = 1;
      break;
    case ENTER:    /***** play selected utterance */
      if (model_item->number_of_samples > 0)
      {
	int width = 42, height = 8, i, j;                  /* ncurses related variables */
	WINDOW *playbackscr = popupWindow (width, height);

	wattrset (playbackscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */
	/*****
	 * draw a box around the dialog window
	 * and empty it.
	 *****/
	werase (playbackscr);
	box (playbackscr, 0, 0);
	for (i = 1; i < width-1; i++)
	  for (j = 1; j < height-1; j++)
	    mvwaddch(playbackscr, j, i, ' ');

	/***** dialog header */

	mvwaddstr(playbackscr, 1, 2, "Playback");
	mvwaddseparator(playbackscr, 2, width);

	/***** play wave if present */

	if (getModelItemSample(model_item, top+current)->has_wav)
	{
	  /***** dialog message */

	  mvwaddstr(playbackscr, 3, 2,"Utterance is being played ...      ");
	  /* mvwaddstrcntr(playbackscr, 6, width, "Press any key to cancel ..."); */

	  wmove(playbackscr, 1, 11); /***** move cursor to an appropriate location */
	  wrefresh (playbackscr);    /***** refresh dialog window */

	  /***** if play fails or was cancelled ....*/

	  if (playSample(getModelItemSample(model_item, top+current)) != AUDIO_OK)
	  {
	    /***** display information */

	    mvwaddstr(playbackscr, 1, 2, "Warning!    ");
	    mvwaddstr(playbackscr, 3, 2,"Either playback has been cancelled  ");
	    mvwaddstr(playbackscr, 4, 2,"or wave couldn't be sent to your    ");
	    mvwaddstr(playbackscr, 5, 2,"soundcard !                         ");
	    mvwaddstrcntr(playbackscr, 6, width, "Press any key to continue ...");

	    wmove(playbackscr, 1, 11); /***** move cursor to an appropriate location */
	    wrefresh (playbackscr);    /***** refresh dialog window */
	    getch();                   /***** wait for keyboard input */
	  }
	}
	else /***** no wave data present! */
	{
	  mvwaddstr(playbackscr, 3, 2,"Utterance can't be played! No wave  ");
	  mvwaddstr(playbackscr, 4, 2,"data available! Note: KvoiceControl ");
	  mvwaddstr(playbackscr, 5, 2,"did NOT save the original wave data!");
	  mvwaddstrcntr(playbackscr, 6, width, "Press any key to cancel ...");

	  wmove(playbackscr, 1, 11); /***** move cursor to an appropriate location */
	  wrefresh (playbackscr);    /***** refresh dialog window */
	  getch();                   /***** wait for keyboard input */
	}

	delwin(playbackscr);   /***** delete ncurses dialog window */
      }
      break;
    case 'r':   /***** record sample utterance */
      {
	int width = 42, height = 8, i, j;   /***** ncurses related variables */
	WINDOW *recordscr = popupWindow (width, height);

	ModelItemSample *new_sample; /***** temporary pointer to new utterance */

	wattrset (recordscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

	/*****
	 * draw a box around the dialog window
	 * and empty it.
	 *****/
	werase (recordscr);
	box (recordscr, 0, 0);
	for (i = 1; i < width-1; i++)
	  for (j = 1; j < height-1; j++)
	    mvwaddch(recordscr, j, i, ' ');

	/***** dialog header */

	mvwaddstr(recordscr, 1, 2, "Recording");
	mvwaddseparator(recordscr, 2, width);

	/***** dialog message */

	mvwaddstr(recordscr, 3, 2,"Utterance is recorded automatically!");
	mvwaddstr(recordscr, 4, 2,"Please say what you want to say ...");
	mvwaddstrcntr(recordscr, 6, width, "Press any key to cancel ...");

	wmove(recordscr, 1, 12); /***** move cursor to a convenient location */
	wrefresh (recordscr);    /***** refresh dialog screen */

	new_sample = recordSample(); /***** auto record a sample utterance */

	if (new_sample != NULL) /***** if the recording was successful ... */
	{
	  appendModelItemSample(model_item, new_sample); /***** add the sample to the model */

	  /***** set cursor to be over new sample utterance */

	  current = model_item->number_of_samples-1 - top;
	  while (current > max_view-1)
	  {
	    current--;
	    top++;
	  }
	}
	else /***** recording has failed or was cancelled */
	{
	  /***** dialog message */

	  mvwaddstr(recordscr, 1, 2, "Cancel!    ");
	  mvwaddstr(recordscr, 3, 2,"Nothing has been recorded! Either   ");
	  mvwaddstr(recordscr, 4, 2,"you cancelled or no data could be   ");
	  mvwaddstr(recordscr, 5, 2,"recorded !                          ");
	  mvwaddstrcntr(recordscr, 6, width, "Press any key to continue ...");

	  wmove(recordscr, 1, 10); /***** move cursor to a convenient location */
	  wrefresh (recordscr);    /***** refresh dialog screen   */
	  getch();                 /***** wait for keyboard input */
	}

	delwin(recordscr);   /***** delete ncurses dialog window */
      }
      break;
    case 'l': /***** edit label */
      {
	/*****
	 * use wstringInput to edit the label of the current utterance
	 * if the label has changed, switch 'modified' to 1
	 *****/
	char tmp_string[1000];
	strcpy(tmp_string, model_item->label);
	free(model_item->label);
	model_item->label = wstringInput(itemscr, 4, 12, 255, input_field_width, tmp_string);
	if (strcmp(tmp_string, model_item->label) != 0)
	  modified = 1;
      }
      break;
    case 'c': /***** edit command */
      {
	/*****
	 * use wstringInput to edit the commandd of the current utterance
	 * if the command has changed, switch 'modified' to 1
	 *****/
	char tmp_string[1000];
	strcpy(tmp_string, model_item->command);
	free(model_item->command);
	model_item->command = wstringInput(itemscr, 5, 12, 255, input_field_width, tmp_string);
	if (strcmp(tmp_string, model_item->command) != 0)
	  modified = 1;
      }
      break;
    case 'd':
      /*****
       * delete currently selected sample utterance from model item
       * and switch 'modified' to 1
       *****/
      deleteModelItemSample(model_item, top+current);
      modified = 1;

      /***** set cursor to a valid value */

      if (top+current == model_item->number_of_samples)
      {
	if (top > 0)
	  top--;
	else if (current > 0)
	  current--;
      }
      break;
    }
  }
}

/********************************************************************************
 * edit speaker model
 ********************************************************************************/

void editModel()
{
  int width = 65, height = 22, i, j;            /***** ncurses related variables */
  WINDOW *editscr = popupWindow(width, height);

  int top = 0, current = 0;  /***** menu selection related variables */
  int max_view = 10;

  int request_finish = 0;    /***** request_finish = 1 breaks the main while loop */

  while (!request_finish)
  {
    wattrset (editscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

    /*****
     * draw a box around the dialog window
     * and empty it.
     *****/
    werase (editscr);
    box (editscr, 0, 0);
    for (i = 1; i < width-1; i++)
      for (j = 1; j < height-1; j++)
	mvwaddch(editscr, j, i, ' ');

    /***** dialog header */

    mvwaddstr(editscr, 1, 2, "Edit Speaker Model");
    mvwaddseparator(editscr, 2, width);

    /***** display help at the bottom */

    mvwaddseparator(editscr, height-4, width);
    mvwaddstr(editscr, height-3, 2, "a = add item, d = delete item, Enter = edit item");
    mvwaddstr(editscr, height-2, 2, "b = back to main menu");

    /***** display information about current speaker model in a table */

    mvwaddstr(editscr, 4, 2, "Number of items:");
    mvwaddint(editscr, 4, 19, model->number_of_items);

    mvwaddstr(editscr, 6, 4, "Label");
    mvwaddch(editscr,  6,25, ACS_VLINE);
    mvwaddstr(editscr, 6,27, "Command");
    mvwaddch(editscr,  6,48, ACS_VLINE);
    mvwaddstr(editscr, 6,50, "Samples");

    /***** display table */

    for (i = 0; i < 55; i++)
      mvwaddch(editscr, 7, 3+i, ACS_HLINE);
    mvwaddch(editscr,  7,25, ACS_PLUS);
    mvwaddch(editscr,  7,48, ACS_PLUS);
    for (i = 0; i < max_view; i++)
    {
      mvwaddch(editscr,  8+i, 25, ACS_VLINE);
      mvwaddch(editscr,  8+i, 48, ACS_VLINE);
    }

    /***** fill table */

    for (i = 0; i < MIN2(model->number_of_items, max_view); i++)
    {
      setHighlight(editscr, active, i == current);
      mvwaddstr(editscr, 8+i,  3, "                      ");
      mvwaddstr(editscr, 8+i, 26, "                      ");
      mvwaddstr(editscr, 8+i, 49, "         ");
      mvwaddnstr(editscr, 8+i,  4, getModelItem(model, top+i)->label, 20);
      mvwaddnstr(editscr, 8+i, 27, getModelItem(model, top+i)->command, 20);
      mvwaddint(editscr, 8+i, 53, getModelItem(model, top+i)->number_of_samples);
    }
    wattroff(editscr, A_REVERSE);

    /***** up/down arrows indicate that not all items can be displayed */

    if (top > 0)
    {
      mvwaddch(editscr, 8, 2, ACS_UARROW);
      mvwaddch(editscr, 8+1, 2, ACS_VLINE);
    }
    if (model->number_of_items > max_view && top+max_view <= model->number_of_items-1)
    {
      mvwaddch(editscr, 8+max_view-2, 2, ACS_VLINE);
      mvwaddch(editscr, 8+max_view-1, 2, ACS_DARROW);
    }

    wmove(editscr, 1,21); /***** move cursor to an appropriate location */
    wrefresh(editscr);    /***** refresh dialog window */

    /* process the command keystroke */

    switch(getch())
    {
    case KEY_UP: /***** cursor up */
      if (top+current > 0)
      {
	if (current > 0)
	 current--;
	else
	  top--;
      }
      break;
    case KEY_DOWN: /***** cursor down */
      if (top+current < model->number_of_items-1)
      {
	if (current < max_view-1)
	 current++;
	else
	  top++;
      }
      break;
    case 'b':
      /*****
       * allow leaving of the menu only if a predefined minimum amount of
       * sample utterances has been donated for each model item
       *****/
      j = MIN_NR_SAMPLES_PER_ITEM;
      for (i = 0; i < model->number_of_items; i++)
	if (getModelItem(model, i)->number_of_samples < j)
	{
	  j = 0;
	  break;
	}
      if (j < MIN_NR_SAMPLES_PER_ITEM) /***** at least one model item doesn't meet requirements */
      {
	int width = 42, height = 10, i, j;             /***** ncurses related variables */
	WINDOW *warnscr = popupWindow (width, height);

	char tmp_string[80];

	wattrset (warnscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

	/*****
	 * draw a box around the dialog window
	 * and empty it.
	 *****/
	werase (warnscr);
	box (warnscr, 0, 0);
	for (i = 1; i < width-1; i++)
	  for (j = 1; j < height-1; j++)
	    mvwaddch(warnscr, j, i, ' ');

	/***** dialog header */

	mvwaddstr(warnscr, 1, 2, "Warning!");
	mvwaddseparator(warnscr, 2, width);

	/***** dialog message */

	sprintf(tmp_string, "We need at least %d sample utterances", MIN_NR_SAMPLES_PER_ITEM);
	mvwaddstr(warnscr, 3, 2, tmp_string);
	mvwaddstr(warnscr, 4, 2,"for each speaker model item!!!");
	mvwaddstr(warnscr, 5, 2,"Please donate the minimum number of ");
	mvwaddstr(warnscr, 6, 2,"samples before leaving this menu!");
	mvwaddstrcntr(warnscr, 8, width, "Press any key to continue ...");

	wmove(warnscr, 1, 11); /***** move cursor to an appropriate location */
	wrefresh (warnscr);    /***** refresh dialog window */

	getch();           /***** wait for keyboard input */
	delwin(warnscr);   /***** delete ncurses dialog window */
      }
      else
	request_finish = 1; /***** otherwise prepare to leave menu */
      break;
    case ENTER: /***** edit the currently selected model item */
      if (model->number_of_items > 0)
	editModelItem(getModelItem(model, top+current));
      break;
    case 'a':   /***** append a new item to the speaker model */
      appendEmptyModelItem(model, "New Item", "New Command");
      modified = 1; /***** switch 'modified' to 1 */

      /***** set cursor to show new item */

      current = model->number_of_items-1 - top;
      while (current > max_view-1)
      {
	current--;
	top++;
      }
      break;
    case 'd':    /***** delete currently selected item from model */
      deleteModelItem(model, top+current);
      modified = 1; /***** switch 'modified' to 1 */

      /***** set cursor to a valid value */

      if (top+current == model->number_of_items)
      {
	if (top > 0)
	  top--;
	else if (current > 0)
	  current--;
      }
      break;
    }
  }
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

  int current         = 0; /***** menu selection related variables */
  int menu_items      = 5;
  enum state status[] = {active, active, active, active, active};

  WINDOW *mainscr; /***** ncurses related variables */
  int i, j;

  /***** load configuration */

  if (loadConfiguration() == 0)
  {
    fprintf(stderr, "couldn't load configuration!");
    exit(-1);
  }

  /***** setup speaker model */

  model = (Model *) malloc(sizeof(Model));
  initModel(model);

  /***** ignore Ctrl-C */

  signal(SIGINT, SIG_IGN);

  /***** ncurses stuff */

  initscr();      /***** initialize the curses library */

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
    mvwaddstrrght(mainscr, 1, COLS, "(c) 2000 Daniel Kiecza");
    mvwaddseparator(mainscr, 2, COLS);
    mvwaddstrcntr(mainscr, 3, COLS, "Speaker Model Editor");
    mvwaddseparator(mainscr, 4, COLS);

    /***** display main menu */

    mvwaddstr(mainscr, 5, 2, "Please Select:");
    mvwaddstr(mainscr, 5, 35, "Model:");

    if (model_file_name != NULL)
      mvwaddstr(mainscr, 5, 42, model_file_name);
    else
      mvwaddstr(mainscr, 5, 42, "(none)");

    setHighlight(mainscr, status[0], current == 0);
    mvwaddstr(mainscr,  7, 5,"New Speaker Model");
    setHighlight(mainscr, status[1], current == 1);
    mvwaddstr(mainscr,  8, 5,"Load Speaker Model");
    setHighlight(mainscr, status[2], current == 2);
    mvwaddstr(mainscr,  9, 5,"Edit Speaker Model");
    setHighlight(mainscr, status[3], current == 3);
    mvwaddstr(mainscr, 10, 5,"Save Speaker Model");
    setHighlight(mainscr, status[4], current == 4);
    mvwaddstr(mainscr, 11, 5,"Exit");

    wmove(mainscr, 5, 17); /***** move cursor to an appropriate location */
    wrefresh(mainscr);     /***** refresh dialog window */

    /* process the command keystroke */

    switch(getch())
    {
    case KEY_UP:   /***** cursor up */
      current = (current == 0 ? menu_items - 1 : current - 1);
      break;
    case KEY_DOWN: /***** cursor down */
      current = (current == menu_items-1 ? 0 : current + 1);
      break;
    case ENTER:    /***** handle menu selections */
    case BLANK:
      switch (current)
      {
      case 0: /***** reset model, but make sure no data gets lost */
	safeResetModel();
	break;
      case 1: /***** load model */
	if (safeResetModel())
	{
	  int max_file_length = 80;
	  char *file_name;
	  char show_file_name[82];

	  int width = 45, height = 11, i, j;             /***** ncurses related variables */
	  WINDOW *loadscr = popupWindow (width, height);

	  wattrset (loadscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL); /***** set bg color of the dialog */

	  /*****
	   * draw a box around the dialog window
	   * and empty it.
	   *****/
	  werase (loadscr);
	  box (loadscr, 0, 0);
	  for (i = 1; i < width-1; i++)
	    for (j = 1; j < height-1; j++)
	      mvwaddch(loadscr, j, i, ' ');

	  /***** dialog header */

	  mvwaddstr(loadscr, 1, 2, "Please enter file name:");
	  mvwaddseparator(loadscr, 2, width);

	  wrefresh (loadscr);
	  wattroff(loadscr, A_BOLD);

	  file_name = wstringInput(loadscr, 4, 2, max_file_length, width-5, ""); /***** get file name */

	  /*****
	   * show_file_name is set to "file" if the file_name is longer
	   * than 15 characters. That way dialog messages stay within the
	   * boundaries of the dialog
	   *****/
	  if (strlen(file_name) <= 15)
	    strcpy(show_file_name, file_name);
	  else
	    strcpy(show_file_name, "file");

	  /***** display information that model is being loaded */

	  wattrset (loadscr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL);
	  mvwaddstr(loadscr, 6,  2, "Loading");
	  mvwaddstr(loadscr, 6, 10, show_file_name);
	  mvwaddstr(loadscr, 6, 11+strlen(show_file_name), "...");
	  wrefresh(loadscr);

	  if (loadModel(model, file_name, 1) == 1) /***** loading model is successful */
	  {
	    mvwaddstr(loadscr, 6, 15+strlen(show_file_name), "success!");

	    /***** set variable model_file_name */

	    if (strstr(file_name, model_file_extension) !=
		file_name+strlen(file_name)-strlen(model_file_extension))
	    {
	      model_file_name = malloc(strlen(file_name) + strlen(model_file_extension) + 1);
	      strcpy(model_file_name, file_name);
	      strcat(model_file_name, model_file_extension);
	    }
	    else
	    {
	      model_file_name = malloc(strlen(file_name) + 1);
	      strcpy(model_file_name, file_name);
	    }
	  }
	  else /***** loading model has failed */
	    mvwaddstr(loadscr, 6, 15+strlen(show_file_name), "failed!");
	  mvwaddstrcntr(loadscr, 8, width,"Press <Space> to return to menu ...");
	  wrefresh(loadscr); /***** refresh dialog */
	  getch();           /***** wait for keyboard input */

	  delwin(loadscr);   /***** delete ncurses dialog window */
	  free (file_name);
	}
	break;
      case 2: /***** edit the speaker model */
	editModel();
	break;
      case 3: /***** save the speaker model */
	if (model->number_of_items > 0)
	{
	  char *file_name;
	  int max_file_length = 84;

	  int width = 45, height = 11, i, j; /***** ncurses related variables */
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

	  /***** display dialog header */

	  mvwaddstr(savescr, 1, 2, "Please enter file name:");
	  mvwaddseparator(savescr, 2, width);

	  wrefresh (savescr);
	  wattroff(savescr, A_BOLD);

	  file_name = wstringInput(savescr, 4, 2, max_file_length,
				   width-5, model_file_name); /***** get file name */

	  /***** display information that model is being saved */

	  wattrset (savescr, color_term ? COLOR_PAIR(2) | A_BOLD : A_NORMAL);
	  mvwaddstr(savescr, 6,  2, "Saving ...");
	  wrefresh(savescr);

	  if (saveModel(model, file_name) == 1) /***** saving successful */
	  {
	    mvwaddstr(savescr, 6, 13, "success!");

	    /***** update model_file_name if necessary */

	    if (model_file_name != NULL && strcmp(model_file_name, file_name) != 0)
	    {
	      free(model_file_name);
	      model_file_name = malloc(strlen(file_name) + 1);
	      strcpy(model_file_name, file_name);
	    }

	    modified = 0; /***** switch modified to '0' */
	  }
	  else /***** saving failed */
	    mvwaddstr(savescr, 6, 13, "failed!");
	  mvwaddstrcntr(savescr, 8, width,"Press <Space> to return to menu ...");
	  wrefresh(savescr); /***** refresh dialog */
	  getch();           /***** wait for keyboard input */

	  delwin(savescr);   /***** delete ncurses dialog window */
	  free (file_name);
	}
	break;
      case 4: /***** quit program */
	if (safeResetModel())
	{
	  wrefresh(mainscr);
  	  request_finish = 1;
	  delwin(mainscr);   /***** delete ncurses dialog window */
	}
	break;
      }
      break;
    }
  }

  endwin(); /***** wrap up ncurses stuff */
  exit(0);  /***** exit program */
}
