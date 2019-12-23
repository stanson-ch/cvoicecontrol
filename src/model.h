/***************************************************************************
                          model.h  -  speaker model
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

#ifndef MODEL_H
#define MODEL_H

#include<preprocess.h>

/* # include<stdlib.h> */
/* # include<stdio.h>  */

/********************************************************************************
 * data structure for a sample utterance
 *
 * data    list of feature vectors, each of size FEAT_VEC_SIZE
 * length  number of feature vectors in 'data'
 * id      'name' of this utterance, usually made up of date and time of donation
 * next    pointer to next sample utterance of the same reference
 * matrix  represents a window of the DTW matrix (used for recognition)
 *         use window width 3, as the warping function has a history depth of 2
 ********************************************************************************/

struct _ModelItemSample
{
  float **data;
  int     length;
  char   *id;

  /***** 'wav present' flag plus data structure to store wav */

  int has_wav;
  int wav_length;
  unsigned char *wav_data;

  struct _ModelItemSample *next;

  float *matrix[3];

  /********************************************************************************
   * tells whether this model item is still active for the current recognition run.
   * an item is deactivated if one of its sample utterances can't be aligned
   * to the test utterance, that's the case when the score of the sample
   * utterance exceeds a threshold or when it can't be aligned due to
   * adjustment window constraints ( ||i(k) - j(k)|| <= r )
   ********************************************************************************/

  int isActive;
};
typedef struct _ModelItemSample ModelItemSample;

/********************************************************************************
 * data structure for a reference item
 * which contains a transcription of the spoken form,
 * a shell command to be executed and a list of
 * sample utterances
 *
 * number_of_samples   number of sample utterances in the list
 * label               transcription of 'what is said'
 * command             this command is executed in case this reference is recognized
 *
 * first               pointer to the first sample utterance in the list
 *
 * next                pointer to the next reference
 ********************************************************************************/

struct _ModelItem
{
  int   number_of_samples;
  char *label;
  char *command;

  ModelItemSample *first;

  struct _ModelItem *next;

};
typedef struct _ModelItem ModelItem;

/********************************************************************************
 * data structure for a speaker model,
 * contains a counter for the number of references in this model
 * and a pointer to the first reference item in the list
 ********************************************************************************/

typedef struct
{
  int number_of_items;

  int total_number_of_sample_utterances;
  int number_of_active_sample_utterances;

  ModelItemSample **direct;
  int *direct_map2ref;

  ModelItem *first;
} Model;

void initModel(Model *model);
void resetModel(Model *model);
int  loadModel(Model *model, char *file_name, int load_wav);
int  saveModel(Model *model, char *file_name);

ModelItem       *getModelItem(Model *model, int idx);
ModelItemSample *getModelItemSample(ModelItem *item, int idx);

void appendModelItemSample(ModelItem *item, ModelItemSample *new_sample);
void deleteModelItemSample(ModelItem *item, int index);

void activateAllSamples();

void appendModelItem(Model *model, ModelItem *new_item);
void appendEmptyModelItem(Model *model, char *label, char *command);
void deleteModelItem(Model *model, int index);

#ifdef MAIN_C
char *model_file_extension = ".cvc";
#else
extern char *model_file_extension;
#endif

#endif
