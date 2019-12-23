/***************************************************************************
                          model.c  -  speaker model
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

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "model.h"

#include "preprocess.h"

/*****
 * a string s is stored in the speaker model file
 * as ('length of %s', %s)!
 *
 * read length of string and string
 *****/

char *fgetstring(char *s, FILE *stream)
{
  int i;

  fread(&i, sizeof(int), 1, stream);
  return fgets(s, i+1, stream);
}

/********************************************************************************
 * initialize a speaker model
 ********************************************************************************/

void initModel(Model *model)
{
  /***** set appropriate initial values */

  model->number_of_items = 0;
  model->first = NULL;

  model->direct = NULL;
  model->direct_map2ref = NULL;
}

/********************************************************************************
 * reset a speaker model to its initial state (empty)
 ********************************************************************************/

void resetModel(Model *model)
{
  ModelItem *tmp_item;
  ModelItem *tmp_item2;
  ModelItemSample *tmp_sample;
  ModelItemSample *tmp_sample2;

  /***** go over the list of reference items: */

  tmp_item = model->first;
  while (tmp_item != NULL)
  {
    /***** release memory allocated for this reference item */

    free(tmp_item->label);
    free(tmp_item->command);

    /***** iterate over the list of sample utterances: */

    tmp_sample = tmp_item->first;
    while (tmp_sample != NULL)
    {
      /***** release memory that has been allocated for this sample utterance */

      int i;

      free(tmp_sample->id);
      for (i = 0; i < tmp_sample->length; i++)
				free(tmp_sample->data[i]);
      free(tmp_sample->data);
      if (tmp_sample->has_wav)
        free(tmp_sample->wav_data);

      for (i = 0; i < 3; i++)
			{
				if (tmp_sample->matrix[i] != NULL)
					free(tmp_sample->matrix[i]);
			}

      tmp_sample2 = tmp_sample;
      tmp_sample  = tmp_sample->next;
      free(tmp_sample2);
    }

    tmp_item2 = tmp_item;
    tmp_item  = tmp_item->next;
    free(tmp_item2);
  }

  model->number_of_items = 0;
  model->first = NULL;

  /***** release memory needed for 'direct access' pointer arrays */

  if (model->direct != NULL)
    free(model->direct);
  model->direct = NULL;

  if (model->direct_map2ref != NULL)
    free(model->direct_map2ref);
  model->direct_map2ref = NULL;
}

/********************************************************************************
 * load a speaker model
 ********************************************************************************/

int loadModel(Model *model, char *file_name, int load_wav)
{
  FILE *fp;    /***** file descriptor for speaker model file */
  int i, j, k; /***** loop variables */

  int direct_pos = 0; /***** position in direct pointer list */

  char tmp_file_name[1000];

  /***** temporary data containers for data read from file */

  char tmp_string[1000];
  char tmp_string2[1000];
  int tmp_int;

  ModelItem *last_item = NULL;

  /***** make sure the speaker model is empty */

  resetModel(model);

  /***** make sure the file name ends in 'model_file_extension' */

  strcpy(tmp_file_name, file_name);
  if (strstr(file_name,model_file_extension) != file_name+strlen(file_name)-strlen(model_file_extension))
    strcat(tmp_file_name, model_file_extension);

  /***** open file in "read mode" and ensure that it is readable */

  if (NULL == (fp = fopen(tmp_file_name, "rb")))
    return 0;

  /*****
   * first line in file contains ID,
   * make sure the file is a proper speaker model file
   *****/

  fgetstring(tmp_string, fp);
  sscanf(tmp_string, "KVoiceControl Speakermodel V%s\n", tmp_string2);
  if (strcmp(tmp_string2, "1.0") != 0)
    return 0;

  /*****
   * read total number of sample utterances
   * which is needed to set up the direct pointer list
   * to the sample utterances
   *****/
  fread(&(model->total_number_of_sample_utterances), sizeof(int), 1, fp);
  model->direct = (ModelItemSample **) malloc(model->total_number_of_sample_utterances *
					      sizeof(ModelItemSample *));
  model->direct_map2ref = (int *)malloc(model->total_number_of_sample_utterances * sizeof(int *));

  /***** next, read number of reference items from file */

  fread(&(model->number_of_items), sizeof(int), 1, fp);

  /***** loop number of reference items: */

  for (i = 0; i < model->number_of_items; i++)
  {
    /***** allocate memory for a new reference item */

    ModelItem *new_item = (ModelItem *) malloc(sizeof(ModelItem));
    ModelItemSample *last_sample = NULL;
		new_item->next = NULL;

    /***** read reference's label and command */

    fread(&tmp_int, sizeof(int), 1, fp);
    new_item->label = (char *)malloc(tmp_int+1);
    fgets(new_item->label, tmp_int+1, fp);
    fread(&tmp_int, sizeof(int), 1, fp);
    new_item->command = (char *)malloc(tmp_int+1);
    fgets(new_item->command, tmp_int+1, fp);

    /***** read number of samples */

    fread(&(new_item->number_of_samples), sizeof(int), 1, fp);

    /***** loop number of samples: */

    for (j = 0; j < new_item->number_of_samples; j++)
    {
      /***** allocate memory for a new sample utterance */

      ModelItemSample *new_sample = (ModelItemSample *) malloc(sizeof(ModelItemSample));
			new_sample->next = NULL;
      model->direct[direct_pos]          = new_sample;
      model->direct_map2ref[direct_pos]  = i;
      direct_pos++;

      /***** read sample's id */

      fread(&tmp_int, sizeof(int), 1, fp);
      new_sample->id = (char *)malloc(tmp_int+1);
      fgets(new_sample->id, tmp_int+1, fp);

      /***** read number of feature vectors (length) in utterance */

      fread(&(new_sample->length), sizeof(int), 1, fp);

      /***** allocate memory for feature vectors */

      new_sample->data = (float**)malloc(sizeof(float*)*new_sample->length);

      /***** read all feature vectors */

      for (k = 0; k < new_sample->length; k++)
      {
				new_sample->data[k] = (float*)malloc(sizeof(float)*FEAT_VEC_SIZE);
				fread(new_sample->data[k], sizeof(float), FEAT_VEC_SIZE, fp);
      }

      /***** load wav data if present (and if requested!) */

      fread(&new_sample->has_wav, sizeof(int), 1, fp);
      if (new_sample->has_wav)
      {
				fread(&new_sample->wav_length, sizeof(int), 1, fp);
				new_sample->wav_data = (unsigned char *)malloc(sizeof(unsigned char)*new_sample->wav_length);
				fread(new_sample->wav_data, sizeof(unsigned char), new_sample->wav_length, fp);

				if (!load_wav)
				{
	  			new_sample->wav_length = 0;
	  			free(new_sample->wav_data);
				}
      }

      /***** allocate three rows of DTW matrix */

      for (k = 0; k < 3; k++)
				new_sample->matrix[k] = (float*)malloc(sizeof(float) * new_sample->length);

      /***** insert sample utterance into reference's list of sample utterances */

      if (last_sample == NULL)
				new_item->first = new_sample;
      else
				last_sample->next = new_sample;
      last_sample = new_sample;
    }

    /***** insert reference into model's list of references */

    if (last_item == NULL)
      model->first = new_item;
    else
      last_item->next = new_item;
    last_item = new_item;
  }

  /***** close file */

  fclose(fp);

  /*fprintf(stderr, "done!\n");*/
  return 1;
}

/********************************************************************************
 * save a speaker model to file
 ********************************************************************************/

int saveModel(Model *model, char *file_name)
{
  /***** file descriptor for speaker model file */
  FILE *f;

  /***** temporary data structures */

  ModelItem *tmp_item;
  ModelItemSample *tmp_sample;
  char *tmp_string;
  int tmp_int;
  int i;

  char tmp_file_name[1000];

  /***** make sure file name ends in "model_file_extension" */

  strcpy(tmp_file_name, file_name);
  if (strstr(file_name,model_file_extension) != file_name+strlen(file_name)-strlen(model_file_extension))
    strcat(tmp_file_name, model_file_extension);

  /***** open file in "write mode" */

  if (NULL == (f = fopen(tmp_file_name, "wb")))
    return 0;

  /***** write "file header" */

  tmp_string = "KVoiceControl Speakermodel V1.0";
  tmp_int = (int)strlen(tmp_string);
  fwrite(&tmp_int, sizeof(int), 1, f);
  fputs(tmp_string, f);

  /***** write total number of sample utterances */

  tmp_int = 0;
  for (i = 0; i < model->number_of_items; i++)
    tmp_int += (getModelItem(model, i))->number_of_samples;
  fwrite(&tmp_int, sizeof(int), 1, f);

  /***** write number of reference items */

  tmp_int = (int)model->number_of_items;
  fwrite(&tmp_int, sizeof(int), 1, f);

  /***** loop all reference items: */

  tmp_item = model->first;
  while (tmp_item != NULL)
  {
    /***** write reference's label and command */

    tmp_int = (int)strlen(tmp_item->label);
    fwrite(&tmp_int, sizeof(int), 1, f);
    fputs(tmp_item->label, f);
    tmp_int = (int)strlen(tmp_item->command);
    fwrite(&tmp_int, sizeof(int), 1, f);
    fputs(tmp_item->command, f);

    /***** write number of samples */

    tmp_int = (int)tmp_item->number_of_samples;
    fwrite(&tmp_int, sizeof(int), 1, f);

    /***** loop all sample utterance of actual reference: */

    tmp_sample = tmp_item->first;
    while (tmp_sample != NULL)
    {
      int i;

      /***** write sample's id */

      tmp_int = (int)strlen(tmp_sample->id);
      fwrite(&tmp_int, sizeof(int), 1, f);
      fputs(tmp_sample->id, f);

      /***** write number of feature vectors */

      tmp_int = (int)tmp_sample->length;
      fwrite(&tmp_int, sizeof(int), 1, f);

      /***** write all feature vectors */

      for (i = 0; i < tmp_sample->length; i++)
      {
	fwrite(tmp_sample->data[i], sizeof(float), VECSIZE, f);
      }

      fwrite(&tmp_sample->has_wav, sizeof(int), 1, f); /***** 'wav present' flag */

      if (tmp_sample->has_wav) /***** save wav if it is present */
      {
	fwrite(&tmp_sample->wav_length, sizeof(int), 1, f);
	fwrite(tmp_sample->wav_data, sizeof(unsigned char), tmp_sample->wav_length, f);
      }

      tmp_sample = tmp_sample->next;
    }
    tmp_item = tmp_item->next;
  }

  /***** close file */

  fclose(f);

  return 1;
}

/********************************************************************************
 * get a reference item from a speaker model by its index
 ********************************************************************************/

ModelItem *getModelItem(Model *model, int idx)
{
  ModelItem *tmp_item;
  int count = 0;

  /***** make sure that index is in range */

  if (idx < 0 || idx >= model->number_of_items)
  {
    fprintf(stderr, "Index out of range!!\n");
    return NULL;
  }

  /***** find 'idx'-th element in list */

  tmp_item = model->first;
  while (tmp_item != NULL && count < idx)
  {
    tmp_item = tmp_item->next;
    count++;
  }

  return tmp_item;
}

/********************************************************************************
 * get a sample utterance from a reference item by its index
 ********************************************************************************/

ModelItemSample *getModelItemSample(ModelItem *item, int idx)
{
  ModelItemSample *tmp_sample;
  int count = 0;

  /***** make sure index is in range */

  if (idx < 0 || idx >= item->number_of_samples)
  {
    fprintf(stderr, "Index out of range!!\n");
    return NULL;
  }

  /***** find 'idx'-th element in list */

  tmp_sample = item->first;
  while (tmp_sample != NULL && count < idx)
  {
    tmp_sample = tmp_sample->next;
    count++;
  }

  return tmp_sample;
}

/********************************************************************************
 * delete a sample
 ********************************************************************************/

void appendModelItemSample(ModelItem *item, ModelItemSample *new_sample)
{
  if (item->first == NULL)
    item->first = new_sample;
  else
  {
    ModelItemSample *tmp_sample = item->first;
    while (tmp_sample->next != NULL)
      tmp_sample = tmp_sample->next;
    tmp_sample->next = new_sample;
  }
  item->number_of_samples++;
}

/********************************************************************************
 * delete a sample
 ********************************************************************************/

void deleteModelItemSample(ModelItem *item, int index)
{
  ModelItemSample *tmp_sample = NULL;
  int i;

  if (item->number_of_samples == 0 || item->first == NULL)
    return;

  tmp_sample = getModelItemSample(item, index);
  if (index == 0)
    item->first = item->first->next;
  else
    getModelItemSample(item, index - 1)->next = tmp_sample->next;

  for (i = 0 ; i < tmp_sample->length; i++)
    free (tmp_sample->data[i]);
  free(tmp_sample->data);

  free (tmp_sample->id);
  if (tmp_sample->has_wav)
    free (tmp_sample->wav_data);
  free(tmp_sample);

  item->number_of_samples--;
}

/********************************************************************************
 * activate all samples in a speaker model for recognition
 ********************************************************************************/

void activateAllSamples(Model *model)
{
  int i;

  for (i = 0; i < model->total_number_of_sample_utterances; i++)
  {
    model->direct[i]->isActive = 1;
  }
  model->number_of_active_sample_utterances = model->total_number_of_sample_utterances;
}

/********************************************************************************
 * append a new item to the model
 ********************************************************************************/

void appendModelItem(Model *model, ModelItem *new_item)
{
  /***** find position in the list and put the item there */

  if (model->first == NULL)
    model->first = new_item;
  else
  {
    ModelItem *tmp_item = model->first;
    while (tmp_item->next != NULL)
      tmp_item = tmp_item->next;
    tmp_item->next = new_item;
  }

  model->number_of_items++; /***** increase number of reference items */
}

/********************************************************************************
 * append a new item to the model
 ********************************************************************************/

void appendEmptyModelItem(Model *model, char *label, char *command)
{
  /***** allocate a new item */

  ModelItem *new_item = (ModelItem *) malloc(sizeof(ModelItem));

  /***** ... set its values */

  new_item->label   = (char *)malloc(strlen(label)+1);
  strcpy(new_item->label, label);
  new_item->command = (char *)malloc(strlen(command)+1);
  strcpy(new_item->command, command);
  new_item->number_of_samples = 0;
  new_item->first             = NULL;
  new_item->next              = NULL;

  /***** and insert it into the list */

  appendModelItem(model, new_item);
}

/********************************************************************************
 * delete an item from the model
 ********************************************************************************/

void deleteModelItem(Model *model, int index)
{
  ModelItem       *tmp_item   = NULL;
  ModelItemSample *tmp_sample = NULL;

  if (model->number_of_items == 0 || model->first == NULL)
    return;

  /***** take item out of the chain of items */

  tmp_item = getModelItem(model, index);
  if (index == 0)
    model->first = model->first->next;
  else
    getModelItem(model, index - 1)->next = tmp_item->next;

  /***** free any memory that was allocated for this item */

  tmp_sample = tmp_item->first;
  while(tmp_sample != NULL)
  {
    ModelItemSample *tmp_sample2 = tmp_sample->next;
    free(tmp_sample);
    tmp_sample = tmp_sample2;
  }

  free (tmp_item->label);
  free (tmp_item->command);
  free (tmp_item);

  model->number_of_items--; /***** decrease number of items in model */
}
