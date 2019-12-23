/***************************************************************************
                          configuration.h  -  configuration file
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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

int mkpath( const char * path, mode_t mode );
int loadConfiguration();

#define CONFIG_DIR	"~/.config/cvoicecontrol"
#define CONFIG_FILE	CONFIG_DIR"/config"

#endif
