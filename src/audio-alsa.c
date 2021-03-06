/***************************************************************************
                          audio.c  -  handle audio device
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <glob.h>
#include <math.h>
#include <alsa/asoundlib.h>

#include "audio.h"
#include "preprocess.h"
#include "keypressed.h"

signed short rec_level, stop_level, silence_level;
int fd_audio;
char *dev_audio;

static snd_pcm_t *capture = NULL;
static int is_open = 0;

/********************************************************************************
 * set name of audio device
 ********************************************************************************/

void setAudio( char *dev )
{
    if ( !dev ) return;
    if ( dev_audio ) free( dev_audio );
    dev_audio = strdup( dev );
    is_open = 0;
}

/********************************************************************************
 * deselect any selected audio device
 ********************************************************************************/

void noAudio(  )
{
    if ( !dev_audio ) return;
    if ( is_open ) closeAudio(  );
    free( dev_audio );
    dev_audio = NULL;
}

/********************************************************************************
 * return name of audio device
 ********************************************************************************/

char *getAudio(  )
{
    return dev_audio;
}

/********************************************************************************
 * check whether device name has been set
 ********************************************************************************/

int audioOK(  )
{
    return dev_audio ? AUDIO_OK : AUDIO_ERR;
}

/********************************************************************************
 * check audio capabilities and initialize audio
 ********************************************************************************/

int initAudio(  )
{
    if ( !dev_audio ) return AUDIO_ERR;
    is_open = 0;
    return AUDIO_OK;
}

/********************************************************************************
 * find list of available audio devices
 ********************************************************************************/

AudioDevices *scanAudioDevices(  )
{
    AudioDevices *devices;

    devices = malloc( sizeof( AudioDevices ) );
    devices->name = malloc( ( sizeof( char * ) ) );
    devices->name[0] = strdup( "default" );
    devices->count = 1;
    return devices;
}

/********************************************************************************
 * connect to audio device (recording)
 ********************************************************************************/

int openAudio(  )
{
    int ret;
    snd_pcm_hw_params_t *hw_params;

    if ( !dev_audio ) return AUDIO_ERR;

    ret = snd_pcm_open( &capture, dev_audio, SND_PCM_STREAM_CAPTURE, 0 );
    if ( ret < 0 )
    {
        fprintf( stderr, "Failed to open capture device %s: %s", dev_audio, snd_strerror( ret ) );
        return AUDIO_ERR;
    }

    snd_pcm_hw_params_alloca( &hw_params );
    snd_pcm_hw_params_any( capture, hw_params );
    snd_pcm_hw_params_set_access( capture, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED );
    snd_pcm_hw_params_set_format( capture, hw_params, SND_PCM_FORMAT_S16_LE );
    snd_pcm_hw_params_set_rate( capture, hw_params, RATE, 0 );
    snd_pcm_hw_params_set_channels( capture, hw_params, CHANNELS );

    ret = snd_pcm_hw_params( capture, hw_params );
    if ( ret < 0 )
    {
        fprintf( stderr, "Can't set hardware parameters %s\n", snd_strerror( ret ) );
        goto out_err;
    }

    ret = snd_pcm_prepare( capture );
    if ( ret < 0 )
    {
        fprintf( stderr, "Can't prepare capture %s\n", snd_strerror( ret ) );
        goto out_err;
    }

    is_open = 1;

    return AUDIO_OK;

 out_err:
    snd_pcm_close( capture );
    return AUDIO_ERR;
}

/********************************************************************************
 * disconnect from audio device (recording)
 ********************************************************************************/

int closeAudio(  )
{
    if ( !is_open ) return AUDIO_OK;
    snd_pcm_close( capture );
    is_open = 0;
    return ( AUDIO_OK );
}

int readAudio( void *buf, size_t size )
{
    if ( !is_open ) return -1;
    int ret = snd_pcm_readi( capture, buf, size / 2 );
    if ( ret < 0 ) return ret;
    if ( ret != size / 2 ) return -1;
    return size;
}

/********************************************************************************
 * get maximum value of a block of recorded audio data
 ********************************************************************************/

int getBlockMax(  )
{
    /*****
     * buffer of size 'frag_size' that contains (raw) data which
     *****/

    unsigned char buffer[FRAG_SIZE];
    int ret, i;

    ret = snd_pcm_readi( capture, buffer, FRAG_SIZE / 2 );
    if ( ret != FRAG_SIZE / 2 )
    {
        if ( ret < 0 ) fprintf( stderr, "Capture error: %s\n", snd_strerror( ret ) );
        else           fprintf( stderr, "Underrun!\n" );
        return -1;
    }

    /***** retrieve max value */

    signed short value, max = 0;
    for ( i = 0; i < FRAG_SIZE / 2; i += 2 )
    {
        value = abs( ( signed short )( buffer[i] | ( buffer[i + 1] << 8 ) ) );
        if ( value > max ) max = value;
    }

    return max;
}

/********************************************************************************
 * estimate channel characteristic (channel mean vector)
 ********************************************************************************/

int calculateChannelMean(  )
{
    float frame[FFT_SIZE];            /***** data containers used for fft calculation */
    int frames_N;                     /***** number of whole (overlapping) frames in current waveform buffer */
    int frameI, i;                    /***** counter variables */
    float feat_vector[FEAT_VEC_SIZE]; /***** preprocessed feature vector */
    int N = 100;
    unsigned char buffer[N * FRAG_SIZE], *b;

    memset( channel_mean, 0, sizeof( channel_mean ) );

    for ( b = buffer, i = 0; i < N; i++, b += FRAG_SIZE )
        if ( snd_pcm_readi( capture, b, FRAG_SIZE / 2 ) != FRAG_SIZE / 2 )
            return AUDIO_ERR;

    /***** do preprocessing and calculate mean vector */
    initPreprocess(  );
    do_mean_sub = 0;

    /*****
     * number of frames that can be extracted from the current amount
     * of audio data
     *****/
    frames_N = ( N * FRAG_SIZE - FFT_SIZE_CHAR ) / OFFSET + 1;

    /***** extract these frames: */

    for ( frameI = 0; frameI < frames_N; frameI++ )
    {
    /***** gather frame */

        for ( i = 0; i < FFT_SIZE; i++ )
            frame[i] =
                ( ( float )
                  ( ( signed short )( buffer[frameI * OFFSET + 2 * i] |
                                      ( buffer[frameI * OFFSET + 2 * i + 1] <<
                                        8 ) ) ) ) * hamming_window[i];

        preprocessFrame( frame, feat_vector );

        for ( i = 0; i < FEAT_VEC_SIZE; i++ ) channel_mean[i] += feat_vector[i];
    }

    for ( i = 0; i < FEAT_VEC_SIZE; i++ ) channel_mean[i] /= frames_N;

    /***** cleanup */

    endPreprocess(  );

    return AUDIO_OK;
}

/********************************************************************************
 * return current channel mean vector
 ********************************************************************************/

const float *getChannelMean(  )
{
    return channel_mean;
}

/********************************************************************************
 * playback utterance to audio device
 ********************************************************************************/

int playUtterance( unsigned char *wav, int length )
{
    int ret;
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *hw_params;

    ret = snd_pcm_open( &pcm, dev_audio, SND_PCM_STREAM_PLAYBACK, 0 );
    if ( ret < 0 )
    {
        fprintf( stderr, "Playback open error: %s\n", snd_strerror( ret ) );
        return AUDIO_ERR;
    }

    snd_pcm_hw_params_alloca( &hw_params );
    snd_pcm_hw_params_any( pcm, hw_params );
    snd_pcm_hw_params_set_rate_resample( pcm, hw_params, 1 );
    snd_pcm_hw_params_set_access( pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED );
    snd_pcm_hw_params_set_format( pcm, hw_params, SND_PCM_FORMAT_S16_LE );
    snd_pcm_hw_params_set_rate( pcm, hw_params, RATE, 0 );
    snd_pcm_hw_params_set_channels( pcm, hw_params, CHANNELS );

    ret = snd_pcm_hw_params( pcm, hw_params );
    if ( ret < 0 )
    {
        fprintf( stderr, "Can't set hardware parameters %s\n", snd_strerror( ret ) );
        goto out_err;
    }

    ret = snd_pcm_writei( pcm, wav, length / 2 );
    if ( ret < 0 )
        ret = snd_pcm_recover( pcm, ret, 0 );
    if ( ret < 0 )
        fprintf( stderr, "Play failed: %s\n", snd_strerror( ret ) );
    else
        snd_pcm_drain( pcm );

    snd_pcm_close( pcm );
    return AUDIO_OK;

 out_err:
    snd_pcm_close( pcm );
    return AUDIO_ERR;
}

/********************************************************************************
 * get an utterance from the sound card via auto recording
 ********************************************************************************/

unsigned char *getUtterance( int *length )
{
    int i;
    signed short value, max = 0;

  /***** set prefetch buffer size to 5, and allocate memory */

    int prefetch_N = 5;
    int prefetch_pos = 0;
    unsigned char prefetch_buf[FRAG_SIZE * prefetch_N];
    void *prefetch = prefetch_buf;

    int count = 0;

  /***** space for one audio block */

    unsigned char buffer_raw[FRAG_SIZE];

  /***** store whole wav data in a queue-like buffer */

    struct Buffer {
        unsigned char buffer[FRAG_SIZE];
        struct Buffer *next;
    };

    int nr_of_blocks = 0;

    struct Buffer *first = NULL;
    struct Buffer *last = NULL;

    unsigned char *return_buffer;

    initKeyPressed(  );

    memset( prefetch_buf, 0x00, sizeof( prefetch_buf ) );

  /***** prefetch data in a circular buffer, and check it for speech content */

    while ( count < CONSECUTIVE_SPEECH_BLOCKS_THRESHOLD )
    {
        if ( keyPressed(  ) )
        {
            return_buffer = NULL;
            goto getUtteranceReturn;
        }

        /* fprintf(stderr, "%d ", fgetc_unlocked(stdin)); */

        if ( snd_pcm_readi( capture, buffer_raw, FRAG_SIZE / 2 ) != FRAG_SIZE / 2 )
        {
            return_buffer = NULL;
            goto getUtteranceReturn;
        }

        memcpy( ( prefetch + prefetch_pos * ( FRAG_SIZE ) ), buffer_raw, FRAG_SIZE );
        prefetch_pos = ( prefetch_pos + 1 ) % prefetch_N;

    /***** check for speech */

        max = 0;
        for ( i = 0; i < FRAG_SIZE / 2 - 1; i += 2 )
        {
            value = abs( ( signed short )( buffer_raw[i] | ( buffer_raw[i + 1] << 8 ) ) );
            if ( value > max )
                max = value;
        }
        if ( max > rec_level ) count++;
        else                   count = 0;
    }

  /***** store prefetch buffer in queue and do recording until level falls below threshold */

    for ( i = prefetch_pos; i < prefetch_pos + prefetch_N; i++ )
    {
        struct Buffer *data = ( struct Buffer * )malloc( sizeof( struct Buffer ) );

        memcpy( data->buffer, prefetch + ( i % prefetch_N ) * FRAG_SIZE, FRAG_SIZE );
        data->next = NULL;
        nr_of_blocks++;

        if ( first == NULL ) first = data;
        if ( last != NULL ) last->next = data;
        last = data;
    }

    count = 0;
    while ( count < CONSECUTIVE_NONSPEECH_BLOCKS_THRESHOLD )
    {
        struct Buffer *data = ( struct Buffer * )malloc( sizeof( struct Buffer ) );

        if ( keyPressed(  ) )
        {
            return_buffer = NULL;
            goto getUtteranceReturn;
        }

        if ( snd_pcm_readi( capture, data->buffer, FRAG_SIZE / 2 ) != FRAG_SIZE / 2 )
        {
            free( data );
            return_buffer = NULL;
            goto getUtteranceReturn;
        }
        data->next = NULL;
        nr_of_blocks++;

        if ( last != NULL ) last->next = data;
        last = data;

    /***** check for nonspeech */

        max = 0;
        for ( i = 0; i < FRAG_SIZE / 2 - 1; i += 2 )
        {
            value = abs( ( signed short )( data->buffer[i] | ( data->buffer[i + 1] << 8 ) ) );
            if ( value > max ) max = value;
        }
        if ( max < stop_level ) count++;
        else                    count = 0;
    }

  /***** assemble data into one buffer, return it */

    return_buffer = ( unsigned char * )malloc( nr_of_blocks * FRAG_SIZE );

    *length = nr_of_blocks * FRAG_SIZE;

    {
        struct Buffer *tmp_buffer = first;

        for ( i = 0; i < nr_of_blocks; i++ )
        {
            memcpy( return_buffer + ( i * FRAG_SIZE ), tmp_buffer->buffer, FRAG_SIZE );
            tmp_buffer = tmp_buffer->next;
        }
    }

 getUtteranceReturn:

    for ( i = 0; i < nr_of_blocks; i++ )
    {
        struct Buffer *tmp_buffer = first;
        first = first->next;
        free( tmp_buffer );
    }

    endKeyPressed(  );
    return return_buffer;
}

/********************************************************************************
 * preprocess an utterance
 ********************************************************************************/

float **preprocessUtterance( unsigned char *wav, int wav_length, int *prep_length )
{
    float frame[FFT_SIZE];            /***** data containers used for fft calculation */
    int frames_N;                     /***** number of whole (overlapping) frames in current waveform buffer */
    int frameI, i;                    /***** counter variables */
    float feat_vector[FEAT_VEC_SIZE]; /***** preprocessed feature vector */

    float **return_buffer;

    initPreprocess(  );

    /*****
     * number of frames that can be extracted from the current amount
     * of audio data
     *****/

    frames_N = ( wav_length - FFT_SIZE_CHAR ) / OFFSET + 1;
    return_buffer = ( float ** )malloc( sizeof( float * ) * frames_N );
    *prep_length = frames_N;

    /***** extract these frames: */

    for ( frameI = 0; frameI < frames_N; frameI++ )
    {
    /***** gather frame */

        for ( i = 0; i < FFT_SIZE; i++ )
            frame[i] =
                ( ( float )
                  ( ( signed short )( wav[frameI * OFFSET + 2 * i] |
                                      ( wav[frameI * OFFSET + 2 * i + 1] <<
                                        8 ) ) ) ) * hamming_window[i];

        preprocessFrame( frame, feat_vector );

        for ( i = 0; i < FEAT_VEC_SIZE; i++ )
            feat_vector[i] -= channel_mean[i];

        return_buffer[frameI] = ( float * )malloc( sizeof( float ) * FEAT_VEC_SIZE );
        memcpy( return_buffer[frameI], feat_vector, sizeof( float ) * FEAT_VEC_SIZE );
    }

    /***** cleanup */

    endPreprocess(  );

    return ( return_buffer );
}
