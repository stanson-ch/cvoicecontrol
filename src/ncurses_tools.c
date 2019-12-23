/***************************************************************************
                          ncurses_tools.c  -  stuff to simplify the use of
                          										the ncurses library
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

/********************************************************************************
 * excerpted from file: widgets.c
 *-------------------------------------------------------------------------------
 * which is part of the ncurses based application
 * hexedit by Adam Rogoyski <apoc@laker.net> Temperanc on EFNet irc
 * Copyright (C) 1998, 1999 Adam Rogoyski
 * released under the GNU General Public License
 ********************************************************************************/

#include <ncurses.h>

#include <stdlib.h>
#include <string.h>

#include "ncurses_tools.h"

/***** default dialog color pair */
#define S_BOX_COLOR  (COLOR_PAIR(3) | A_BOLD)

/********************************************************************************
 * tells whether a character is printable
 ********************************************************************************/

int isprintable(int c)
{
  return ((c > 32) && (c < 127));
}

/********************************************************************************
* output a string horizontally centered in a window
********************************************************************************/

int mvwaddstrcntr(WINDOW *window, int y, int width, char *text)
{
  int x = width/2 - (strlen(text)/2);
  return(mvwaddstr(window, y, x, text));
}

/********************************************************************************
* align a string horizontally to the right edge of a window
********************************************************************************/

int mvwaddstrrght(WINDOW *window, int y, int width, char *text)
{
  int x = width - strlen(text) - 2;
  return(mvwaddstr(window, y, x, text));
}

/********************************************************************************
 * output an integer value in a window
 ********************************************************************************/

int mvwaddint(WINDOW *window, int y, int x, int i)
{
  char tmp_string[200];
  sprintf(tmp_string, "%d", i);
  return(mvwaddstr(window, y, x, tmp_string));
}

/********************************************************************************
 * output a horizontal line that uses RTEE and LTEE to connect to a surrounding
 * box
 ********************************************************************************/

int mvwaddseparator(WINDOW *window, int y, int width)
{
  int i;

  mvwaddch (window, y, 0,      ACS_LTEE);
  for (i = 1; i < width-1; i++)
    mvwaddch (window, y, i, ACS_HLINE);
  mvwaddch (window, y, width-1, ACS_RTEE);

  return 1;
}

/********************************************************************************
 * Generic blank popup window with a border. Comes out centered.
 ********************************************************************************/

WINDOW *popupWindow (int width, int height)
{
  int winy = 0, winx = 0;
  WINDOW *wpopup = NULL;

  if (LINES < height)
    height = LINES;
  if (COLS < width)
    width = COLS;

  winy = (LINES / 2) - (height / 2);
  winx = (COLS / 2) - (width / 2);
  wpopup = newwin (height, width, winy, winx);
  if (wpopup == NULL)
  {
    fprintf (stderr, "Cannot open up popup window\n");
    return NULL;
  }
  scrollok (wpopup, FALSE);
  box (wpopup, 0, 0);
  return wpopup;
}

/********************************************************************************
 * Creates an entry box to type in a string and returns it.
 * The caller is responsible for freeing the returned pointer.
 ********************************************************************************/

char *wstringInput(WINDOW *win, int y, int x, int len, int max, char *sample)
{
  char *str = NULL;
  char *retstr = NULL;
  int done = 0;
  int i = 0;
  int n = 0;
  int oldx = 0;
  int left = 0;
  int right = max - 1;
  int samplelen = strlen (sample ? sample : "");
  wchar_t in = 0;

  wmove (win, y, x);
  wattrset (win, color_term ? S_BOX_COLOR : A_REVERSE);
  str = (char *)malloc(len + 2);
  if (!str)
    return NULL;

  *(str + len + 1) = '\0';
  memset (str, ' ', len + 1);
  strncpy (str, sample ? sample : "", samplelen);
  for (i = 0; i < max; i++)
  {
    if (*(str + i) && (i < max))
    {
      wprintw (win, "%c", *(str + i));
    }
    else
      wprintw (win, " ");
  }
  i = samplelen;
  if (i >= max)
    i = max - 1;
  wmove (win, y, x + i);
  wrefresh (win);

  while (!done && ((in = getch ()) != '\n'))
  {
    switch (in)
    {
    case CONTROL_C:
    case CONTROL_G:
    case CONTROL_X:
    case ESCAPE_CHARACTER:
      *str = '\0';
      done = 1;
      break;
    case KEY_LEFT:
      if (i > 0)
      {
	if (win->_curx > x)
	  /* not at left hand side */
	{
	  i--;
	  wmove (win, y, win->_curx - 1);
	}
	else if (i > 0)
	  /* left hand side */
	{
	  i--;
	  if (left > 0)
	  {
	    left--;
	    right--;
	  }
	  wmove (win, y, x);
	  for (n = left; n <= right; n++)
	  {
	    if ((n < len) && *(str + n))
	      wprintw (win, "%c", *(str + n));
	    else
	      wprintw (win, " ");
	  }
	  wmove (win, y, x);
	}

      }
      break;
    case KEY_RIGHT:
      if (win->_curx < (x + max - 1))
	/* not at right hand side */
      {
	i++;
	wmove (win, y, win->_curx + 1);
      }
      else if (i < len - 1)
	/* right hand side */
      {
	i++;
	if (right < len - 1)
	{
	  left++;
	  right++;
	}
	wmove (win, y, x);
	for (n = left; n <= right; n++)
	{
	  if (*(str + n))
	    wprintw (win, "%c", *(str + n));
	  else
	    wprintw (win, " ");
	}
	wmove (win, y, x + max - 1);
      }
      break;
    case BACKSPACE:
    case CONTROL_H:
    case KEY_BACKSPACE:
    case KEY_DC:
#ifdef __PDCURSES__
    case CTL_BKSP:
#endif
      if (i > 0)
      {
	if (win->_curx > x)
	  /* not at left hand side */
	{
	  i--;
	  for (n = strlen (str); n < len; n++)
	    *(str + n) = ' ';
	  for (n = i; n < len; n++)
	    *(str + n) = *(str + n + 1);
	  *(str + n) = ' ';
	  oldx = win->_curx;
	  wmove (win, y, x);
	  for (n = left; n <= right; n++)
	  {
	    if (*(str + n))
	      wprintw (win, "%c", *(str + n));
	    else
	      wprintw (win, " ");
	  }
	  wmove (win, y, oldx - 1);
	}
	else
	  /* at left hand side, not begining of buffer */
	{
	  i--;
	  for (n = i; n < len - 1; n++)
	    *(str + n) = *(str + n + 1);
	  if (left > 0)
	  {
	    left--;
	    right--;
	  }
	  wmove (win, y, x);
	  for (n = left; n <= right; n++)
	  {
	    if (*(str + n))
	      wprintw (win, "%c", *(str + n));
	    else
	      wprintw (win, " ");
	  }
	  wmove (win, y, x);
	}
      }
      else
	beep();
      break;
    case BLANK:
      oldx = win->_curx;
      if ((win->_curx < x + max - 1)
	  && (i < len - 1) && (*(str + len - 1) <= ' '))
      {
	for (n = len - 1; n > i; n--)
	{
	  *(str + n) = *(str + n - 1);
	}
	*(str + i) = ' ';
	*(str + len) = ' ';
	i++;
	wmove (win, y, x);
	for (n = left; n <= right; n++)
	{
	  if (*(str + n))
	    wprintw (win, "%c", *(str + n));
	  else
	    wprintw (win, " ");
	}
	wmove (win, y, oldx + 1);
      }
      else if ((win->_curx == x + max - 1)
	       && (i < len - 1) && (*(str + len - 1) <= ' '))
      {
	for (n = len - 1; n > i; n--)
	{
	  *(str + n) = *(str + n - 1);
	}
	*(str + i) = ' ';
	*(str + len) = ' ';
	i++;
	if (right < len - 1)
	{
	  left++;
	  right++;
	}
	wmove (win, y, x);
	for (n = left; n <= right; n++)
	{
	  if (*(str + n))
	    wprintw (win, "%c", *(str + n));
	  else
	    wprintw (win, " ");
	}
	wmove (win, y, x + max - 1);
      }
      else if ((win->_curx == x + max - 1)
	       && (i == len - 1))
      {
	*(str + len - 1) = ' ';
	wmove (win, y, x + max - 1);
	wprintw (win, " ");
	wmove (win, y, x + max - 1);
	beep();
      }
      else
	beep();
      break;

      /* erase whole line and bring cursor back to begining */
    case CONTROL_U:
      memset (str, 0x00, len);

      wmove (win, y, x);
      for (i = 0; i < max; i++)
	wprintw (win, " ");
      i = 0;
      left = 0;
      right = max - 1;
      wmove (win, y, x);
      wrefresh (win);
      break;

    default:			/* normal input */
      if (isprintable (in))
      {
	if (win->_curx != (x + max - 1))
	  /* not at right hand side */
	{
	  *(str + i) = in;
	  i++;
	  oldx = win->_curx;
	  wmove (win, y, x);
	  for (n = left; n <= right; n++)
	  {
	    if (*(str + n))
	      wprintw (win, "%c", *(str + n));
	    else
	      wprintw (win, " ");
	  }
	  wmove (win, y, oldx + 1);
	}
	else if (i < len - 1)
	  /* right hand side, not end of buffer */
	{
	  *(str + i) = in;
	  i++;
	  if (right < len - 1)
	  {
	    left++;
	    right++;
	  }
	  wmove (win, y, x);
	  for (n = left; n <= right; n++)
	  {
	    if (*(str + n))
	      wprintw (win, "%c", *(str + n));
	    else
	      wprintw (win, " ");
	  }
	  wmove (win, y, x + max - 1);
	}
	else if (i == len - 1)
	  /* right hand side, end of buffer */
	{
	  *(str + i) = in;
	  beep();
	  wmove (win, y, x);
	  for (n = left; n <= right; n++)
	  {
	    if (*(str + n))
	      wprintw (win, "%c", *(str + n));
	    else
	      wprintw (win, " ");
	  }
	  wmove (win, y, x + max - 1);
	}
      }
    }
    wrefresh (win);
  }

  i = len;
  while(i >= 0 && str[i] == ' ')
  {
    str[i] = '\0';
    i--;
  }

  retstr = (char *)malloc(strlen(str) + 1);
  strcpy(retstr, str);
  free(str);
  return retstr;
}

