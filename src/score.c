/***************************************************************************
                          score.c  -  data structure to handle recognition
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

#include "score.h"

#include <stdlib.h>

/*
# include <sys/types.h>
# include <unistd.h>
*/

/********************************************************************************
 * initialize the score queue
 ********************************************************************************/

void initScoreQueue(ScoreQueue *queue)
{
  queue->length = 0;    /***** set reasonable initial values */
  queue->first  = NULL;
};

/********************************************************************************
 * empty the score queue
 ********************************************************************************/

void resetScoreQueue(ScoreQueue *queue)
{
  /***** while there are elements in the queue left ... */

  while (queue->first != NULL)
  {
    /***** ... remove the first element */

    ScoreQueueItem *tmp_item = queue->first;
    queue->first = queue->first->next;
    free(tmp_item);
  }

  queue->length = 0; /***** queue is empty now */
}

/********************************************************************************
 * insert a hypothesis into the score queue
 ********************************************************************************/

void insertInScoreQueue(ScoreQueue *queue, float score, int ref_index)
{
  /***** allocate a new score object according to the specified parameters */

  ScoreQueueItem *new_score = (ScoreQueueItem *)malloc(sizeof(ScoreQueueItem));
  new_score->score = score;
  new_score->id    = ref_index;
  new_score->next  = NULL;

  /*****
   * find the appropriate place in the queue,
   * so that the queue is sorted by increasing scores
   *****/
  if (queue->first == NULL)               /***** first case: queue is empty */
    queue->first = new_score;
  else if (queue->first->score >= score)  /***** second case: new item is the new head item of the queue */
  {
    new_score->next = queue->first;
    queue->first = new_score;
  }
  else                                    /***** third case: item has to be put inside the queue */
  {
    ScoreQueueItem *tmp_queue = queue->first;

    while (tmp_queue->next != NULL && tmp_queue->next->score < score)
      tmp_queue = tmp_queue->next;
    new_score->next = tmp_queue->next;
    tmp_queue->next = new_score;
  }

  queue->length++; /***** adjust length of queue */
}


/********************************************************************************
 * interpret contents of score_queue
 ********************************************************************************/

int getResultID(ScoreQueue *queue)
{
    int id = -1;

  /*****
   * recognition of reference 'ref' is successful iff:
   * - first three score queue item have 'item->id == ref' OR
   * - first two  -- '' --                                 AND
   *   the remaining 'item->id == ref' are in the first half of the list
   *****/

    if (queue->length == 0)
    {
	id = -1;
    }
    else if (queue->length == 1)
    {
	id = queue->first->id;
    }
    else if (queue->length == 2)
    {
	if (queue->first->id == queue->first->next->id)
	{
	    id = queue->first->id;
	}
	else
	{
	    id = -1;
	}
    }
    else if (queue->length >= 3)
    {
	if (queue->first->id == queue->first->next->id &&
	    queue->first->next->id == queue->first->next->next->id)
	{
	    id = queue->first->id;
	}
	else if (queue->first->id == queue->first->next->id)
	{
	    if (1.0*(queue->first->next->next->score - queue->first->next->score) >=
		2.0*(queue->first->next->score - queue->first->score))
	    {
		id = queue->first->id;
	    }
	    else
	    {
		ScoreQueueItem *tmp_item = queue->first->next->next;
		int ok = 1;
		int count = 3;
		while (tmp_item != NULL)
		{
		    if (tmp_item->id == queue->first->id && count > queue->length/2)
		    {
			ok = 0;
			break;
		    }

		    tmp_item = tmp_item->next;
		    count++;
		}

		if (ok)
		{
		    id = queue->first->id;
		}
		else
		{
		    id = -1;
		}
	    }
	}
	else
	{
	    id = -1;
	}
    }

  /*
  in client/server setup broadcast results to listening clients from here?
  */

    return id;
}
