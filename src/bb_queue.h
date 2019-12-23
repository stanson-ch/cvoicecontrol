/***************************************************************************
                          bb_queue.h  -  queue for branch and bound
                          							 recognition method
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

#ifndef BB_QUEUE_H
#define BB_QUEUE_H

/*****
  a queue needed for branchNbound decoding
  *****/
struct _BBQueueItem
{
  int   pos;
  float score;
  int   sample_index;

  struct _BBQueueItem *next;
};
typedef struct _BBQueueItem BBQueueItem;

typedef struct
{
  int length;

  BBQueueItem *first;
} BBQueue;

void initBBQueue(BBQueue *queue);
void resetBBQueue(BBQueue *queue);
void insertItemIntoBBQueue(BBQueue *queue, BBQueueItem *item);
void insertIntoBBQueue(BBQueue *queue, int pos, float score, int sample_index);
BBQueueItem *headBBQueue(BBQueue *queue);

#endif

