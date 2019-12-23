/***************************************************************************
                          score.h  -  data structure to handle recognition
                          						results
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

#ifndef SCORE_H
#define SCORE_H

#include "model.h"

/****
  a queue of recognition scores,
  the queue elements are sorted by increasing
  score. The final recognition result can be
  obtained from this queue
  *****/
struct _ScoreQueueItem
{
  float score;
  int   id;
  struct _ScoreQueueItem *next;
};
typedef struct _ScoreQueueItem ScoreQueueItem;

typedef struct
{
  int length;

  ScoreQueueItem *first;
} ScoreQueue;

void initScoreQueue(ScoreQueue *queue);
void resetScoreQueue(ScoreQueue *queue);
void insertInScoreQueue(ScoreQueue *queue, float score, int ref_index);

int getResultID(ScoreQueue *queue);

#endif

