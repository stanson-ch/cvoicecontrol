/***************************************************************************
                          keypressed.c  -  handle asynchronous keyboard
                                           input
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
#include <stdio.h>

#include <termios.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/time.h>

#include "keypressed.h"

static  struct termios stored_settings;

int    fd_stdin;
fd_set reads_keypressed;

/********************************************************************************
 * switch terminal to non-canonical mode, returning every key press
 ********************************************************************************/

void set_keypress(void)
{
  struct termios new_settings;

  tcgetattr(0,&stored_settings);

  new_settings = stored_settings;

  /* Disable canonical mode, and set buffer size to 1 byte */
  new_settings.c_lflag &= (~ICANON);
  new_settings.c_cc[VTIME] = 0;
  new_settings.c_cc[VMIN] = 1;

  tcsetattr(0,TCSANOW,&new_settings);
  return;
}

/********************************************************************************
 * reset terminal to canonical mode, returning keyboard buffer
 * after '\n' is pressed
 ********************************************************************************/

void reset_keypress(void)
{
  tcsetattr(0,TCSANOW,&stored_settings);
  return;
}

/********************************************************************************
 * initialize console to allow calls to keyPressed()
 ********************************************************************************/

int initKeyPressed()
{
  struct timeval time;
  time.tv_sec = 1;
  time.tv_usec= 0;

  set_keypress();

  if ((fd_stdin = open("/dev/stdin", O_RDONLY, 0)) == -1)
    return 0;

  FD_SET(fd_stdin, &reads_keypressed);

  if (select(fd_stdin+1, &reads_keypressed, NULL, NULL, &time) == -1)
  {
    close(fd_stdin);
    return 0;
  }

  return 1;
}

/********************************************************************************
 * check whether key was pressed on keyboard
 ********************************************************************************/

int keyPressed()
{
  char buffer;

  /***** return true if there is incoming data ... */

  if ((FD_ISSET(fd_stdin, &reads_keypressed)) && (read(fd_stdin, &buffer, 1) > 0))
    return 1;
  else
    return 0;
}

/********************************************************************************
 * reset console to default behaviour
 ********************************************************************************/

void endKeyPressed()
{
  FD_ZERO(&reads_keypressed);
  reset_keypress();
}
