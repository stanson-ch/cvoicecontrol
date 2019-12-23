/***************************************************************************
                          ncurses_tools.h  -  stuff to simplify the use of
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

#ifndef CURSES_TOOLS_H
#define CURSES_TOOLS_H

#ifndef CTRL
#define CTRL(x)		((x) & 0x1f)
#endif

#define QUIT		CTRL('Q')
#define ESCAPE		CTRL('[')
#define ENTER		'\n'

#define  ESCAPE_CHARACTER  27
#define  BACKSPACE        127
#define  CONTROL_A          1
#define  CONTROL_B          2
#define  CONTROL_C          3
#define  CONTROL_D          4
#define  CONTROL_E          5
#define  CONTROL_F          6
#define  CONTROL_G          7
#define  CONTROL_H          8
#define  CONTROL_I          9
#define  CONTROL_L         12
#define  CONTROL_N         14
#define  CONTROL_O         15
#define  CONTROL_P         16
#define  CONTROL_R         18
#define  CONTROL_T         20
#define  CONTROL_U         21
#define  CONTROL_V         22
#define  CONTROL_W         23
#define  CONTROL_X         24
#define  CONTROL_Y         25
#define  BLANK             32


char *wstringInput(WINDOW *win, int y, int x, int len, int max, char *sample);
WINDOW *popupWindow (int width, int height);
int isprintable(int c);
int mvwaddstrcntr(WINDOW *window, int y, int width, char *text);
int mvwaddstrrght(WINDOW *window, int y, int width, char *text);
int mvwaddseparator(WINDOW *window, int y, int width);
int mvwaddint(WINDOW *window, int y, int x, int i);

#ifdef MAIN_C
int color_term = 0;
#else
extern int color_term;
#endif

#endif

