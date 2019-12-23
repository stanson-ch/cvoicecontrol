/***************************************************************************
                          bb_queue.c  -  queue for branch and bound
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

#include "bb_queue.h"

#include <stdlib.h>

/********************************************************************************
 * initialize queue
 ********************************************************************************/

void initBBQueue(BBQueue *queue)
{
  queue->length = 0;
  queue->first  = NULL;
};

/********************************************************************************
 * reset (empty) queue
 ********************************************************************************/

void resetBBQueue(BBQueue *queue)
{
  while (queue->first != NULL)
  {
    BBQueueItem *tmp_item = queue->first;
    queue->first = queue->first->next;
    free(tmp_item);
  }
  queue->length = 0;
};

/********************************************************************************
 * insert an item into queue (items are sorted by increasing score!)
 ********************************************************************************/

void insertItemIntoBBQueue(BBQueue *queue, BBQueueItem *item)
{
  if (queue->first == NULL)
    queue->first = item;
  else if (queue->first->score >= item->score)
  {
    item->next = queue->first;
    queue->first = item;
  }
  else
  {
    /***** find place in queue */

    BBQueueItem *tmp_queue = queue->first;

    while (tmp_queue->next != NULL && tmp_queue->next->score < item->score)
      tmp_queue = tmp_queue->next;
    item->next = tmp_queue->next;
    tmp_queue->next = item;
  }

  queue->length++;
}

/********************************************************************************
 * insert an item into queue (items are sorted by increasing score!)
 ********************************************************************************/

void insertIntoBBQueue(BBQueue *queue, int pos, float score, int sample_index)
{
  BBQueueItem *new_item = (BBQueueItem *)malloc(sizeof(BBQueueItem));
  new_item->pos          = pos;
  new_item->score        = score;
  new_item->sample_index = sample_index;
  new_item->next         = NULL;

  insertItemIntoBBQueue(queue, new_item);
}

/********************************************************************************
 * get the head item from the queue
 ********************************************************************************/

BBQueueItem *headBBQueue(BBQueue *queue)
{
  BBQueueItem *retval = queue->first;
  queue->first = queue->first->next;
  queue->length--;

  return(retval);
}


