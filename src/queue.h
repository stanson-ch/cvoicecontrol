/***************************************************************************
                          queue.h  -  simple thread safe queue
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

#ifndef QUEUE_H
#define QUEUE_H

#include "semaphore.h"

/*****
  states a queue item can have
  *****/
enum QStatus {Q_invalid, Q_start, Q_data, Q_end, Q_abort, Q_exit};

/*****
  one item of the queue consists of
  a chunk of data, a status flag and
  a pointer to the next queue item
  *****/
struct _QueueItem
{
  void              *data;
  struct _QueueItem *next;
  enum QStatus       status;
};
typedef struct _QueueItem QueueItem;

/*****
  a queue knows the 'number_of_elements' it contains.
  It has a pointer to the 'head' item and the 'tail' item
  type indicates the type of data stored in the queue
  (needed for proper memory allocation etc.)
  *****/
typedef struct
{
  int number_of_elements;
  QueueItem *head;
  QueueItem *tail;
  enum {T_invalid, T_char, T_float} type;

  Semaphore semaphore;     /***** needed to block reader thread if queue is empty */
  pthread_mutex_t access;  /***** ensure thread-safe access to queue */
} Queue;

/********************************************************************************
 * initialize a queue that contains data of type 'type'
 ********************************************************************************/

void initQueue(Queue *queue, char *type)
{
  /*****
    initially, queue contains no elements, head and tail point to NULL
    and the requested type is setup, unless it is not supported!
    *****/
  queue->number_of_elements = 0;
  queue->head = NULL;
  queue->tail = NULL;
  if (strcmp(type, "char") == 0)
    queue->type = T_char;
  else if (strcmp(type, "float") == 0)
    queue->type = T_float;
  else
  {
    queue->type = T_invalid;
    fprintf(stderr, "Type not supported!\n");
  }

  semaphore_init( &queue->semaphore );
  semaphore_down( &queue->semaphore );

  pthread_mutex_init(&queue->access, NULL);
}

/********************************************************************************
 * remove all elements from a queue
 ********************************************************************************/

void resetQueue(Queue *queue)
{
  QueueItem *item;

  pthread_mutex_lock( &queue->access );

  /*****
    while the queue still contains items
    delete the item at the head of the queue
    *****/
  while (queue->number_of_elements > 0)
  {
    item = queue->head;
    queue->head = item->next;
    free(item->data);
    free(item);
    queue->number_of_elements--;
  }

  /*****
    reset head and tail to NULL pointer
   *****/
  queue->head = NULL;
  queue->tail = NULL;

  semaphore_destroy( &queue->semaphore );
  semaphore_init( &queue->semaphore );
  semaphore_down( &queue->semaphore );

  pthread_mutex_unlock( &queue->access );
}

/********************************************************************************
 * append a new item to a queue
 ********************************************************************************/

void enqueue(Queue *queue, void *data, int size, int _status)
{
  /*****
    allocate memore for new queue item
    *****/
  QueueItem *new_item = (QueueItem *) malloc(sizeof(QueueItem));

  pthread_mutex_lock( &queue->access );   /***** get exclusive access */

  /*****
    status of the queue item
    *****/
  new_item->status = _status;

  /*****
    allocate memory for item's data depending on its type,
    then copy 'data' into the allocated space
    *****/
  switch (queue->type)
  {
  case T_char:
    new_item->data = malloc(size*sizeof(char));
    memcpy(new_item->data, data, size*sizeof(char));
    break;
  case T_float:
    new_item->data = malloc(size*sizeof(float));
    memcpy(new_item->data, data, size*sizeof(float));
    break;
  case T_invalid:
  default:
    fprintf(stderr, "Type not supported by queue!\n");
  }

  /*****
    append new item to tail of queue
    *****/
  new_item->next = NULL;

  if (queue->number_of_elements > 0)
  {
    queue->tail->next = new_item;
    queue->tail = new_item;
  }
  else
  {
    queue->head = new_item;
    queue->tail = new_item;
  }

  /*****
    increase queue element counter by one
    *****/
  queue->number_of_elements++;

  semaphore_up( &queue->semaphore ); /***** new element in queue! */

  pthread_mutex_unlock( &queue->access );   /***** release exclusive access */
}

/********************************************************************************
 * remove an item from the head of a queue
 ********************************************************************************/

void *dequeue(Queue *queue, enum QStatus *status)
{
  void *retval = NULL;

  semaphore_down( &queue->semaphore );  /***** block if queue empty! */

  pthread_mutex_lock( &queue->access ); /***** get exclusive access */

  if (queue->number_of_elements > 1)
  {
    QueueItem *dequeue_item = queue->head;
    queue->head = queue->head->next;
    queue->number_of_elements--;

    retval = dequeue_item->data;
    *status = dequeue_item->status;
    free(dequeue_item);
  }
  else if (queue->number_of_elements == 1)
  {
    retval = queue->head->data;
    *status = queue->head->status;
    free(queue->head);

    queue->head               = NULL;
    queue->tail               = NULL;
    queue->number_of_elements = 0;
  }
  else
    status = Q_invalid;

  pthread_mutex_unlock( &queue->access );   /***** release exclusive access */

  return retval;
}

/********************************************************************************
 * get length of queue
 ********************************************************************************/

int numberOfElements(Queue *queue)
{
  int retval;

  pthread_mutex_lock( &queue->access );   /***** get exclusive access */
  retval = queue->number_of_elements;
  pthread_mutex_unlock( &queue->access ); /***** release exclusive access */

  return retval;
}

#endif

