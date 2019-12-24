/***************************************************************************
                          cvoicecontrol.c  -  a simple speech recognizer
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

#define MAIN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <float.h>

#include <pthread.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/soundcard.h>

#ifdef _AIX
#include <sys/select.h>
#endif

#include <getopt.h>

#include "cvoicecontrol.h"

#include "model.h"
#include "configuration.h"

#include "queue.h"
#include "semaphore.h"

#include "score.h"
#include "bb_queue.h"

#include "audio.h"
#include "mixer.h"
#include "preprocess.h"

#include "../config.h"

Model *model;                                    /* speaker model */

Queue queue1;                                    /* thread-safe queue used to hand data from 'recording' to 'preprocessing' */
Queue queue2;                                    /* thread-safe queue used to hand data from 'preprocessing' to 'recognition' */

ScoreQueue score_queue;

/*
 * forward declaration:
 * each of these three functions is run in a separate thread
 * these threads are connected via thread safe data queues.
 * Together the three threads form the whole recognizer
 */
void record( void );
void preprocess( void );
void recognize( void );

/*
 * status of the audio recording thread (plus mutex variables)
 */
enum AudioStatus {
    A_invalid,
    A_off,
    A_prefetching,
    A_recording,
    A_aborting,
    A_exiting
} A_status = A_invalid;

pthread_mutex_t mutex_A_status;                  /* ensure thread-safe access to A_status */
pthread_cond_t cond_A_status;                    /* wait condition for A_status change */
Semaphore auto_recording;                        /* used to switch audio thread to auto recording mode */

/*
 * indicator whether preprocessing of current utterance is finished
 * (used to switch to faster branch&bound decoding at the right time)
 * plus mutex for thread-safe access
 */
int P_done;
pthread_mutex_t mutex_P_done;

/* terminates server program, if set to 0 */
int running = 1;

/*
 * if set, program exits after one successful recognition
 * pass. Can be set via command line option (--once)
 */
int run_once = 0;

/* ID of the recognition result. For use in shell scripts */
int result_id = 0;

void beep(  )
{
    /* fprintf(stderr, "BEEP\n"); */
}

int g_verbose = 0;
int g_daemon = 0;

/********************************************************************************
 * thread safe changing of the status of the audio recording thread
 ********************************************************************************/

enum AudioStatus setAudioStatus( enum AudioStatus s )
{
    int retval = A_invalid;                      /* return value */

    /* get exclusive access to audio status variable */
    pthread_mutex_lock( &mutex_A_status );

    /*
     * reaction depends on
     * - what state the caller requests (via 's') AND
     * - what state the audio thread is currently in!
     */

    switch ( s )
    {
        case A_off:
            /* ok ? */
            A_status = A_off;
            retval = A_off;
            break;

        case A_prefetching:
            /* switch to prefetching only if the audio thread is in a passive state currently */
            if( A_status == A_off || A_status == A_invalid )
            {
                A_status = A_prefetching;
                retval = A_prefetching;
                semaphore_up( &auto_recording );
                break;
            }
            /* ... otherwise just return 'invalid' */
            retval = A_invalid;
            break;

        case A_recording:
            /* switch to recording only if the audio thread was in prefetching mode before */
            if( A_status == A_prefetching )
            {
                A_status = A_recording;
                retval = A_recording;
                break;
            }
            /* ... otherwise just return 'invalid' */
            retval = A_invalid;
            break;

        case A_aborting:
            /* ignore request for aborting if thread is passive already */
            if( A_status == A_off || A_status == A_invalid )
            {
                retval = A_off;
                break;
            }
            /* otherwise switch to aborting mode */
            A_status = A_aborting;
            retval = A_aborting;
            break;

        case A_exiting:
            retval = A_exiting;
            A_status = A_exiting;
            semaphore_up( &auto_recording );
            break;

        case A_invalid:
            /* invalid: change nothing! */
            retval = A_invalid;
            break;
    }

    if( g_verbose ) printf( "status: %d\n", A_status );

    /* give up exclusive access to audio status */
    pthread_mutex_unlock( &mutex_A_status );
    pthread_cond_broadcast( &cond_A_status );
    return retval;
}

/********************************************************************************
 * thread safe reading of the audio status
 ********************************************************************************/

enum AudioStatus getAudioStatus(  )
{
    enum AudioStatus retval;                     /* return value */

    pthread_mutex_lock( &mutex_A_status );       /* get exclusive access to audio status variable */
    retval = A_status;                           /* put status of audio thread in return variable */
    pthread_mutex_unlock( &mutex_A_status );     /* give up exclusive access to audio status */

    return retval;
}

void waitForAudioStatus( enum AudioStatus s )
{
    pthread_mutex_lock( &mutex_A_status );
    while( A_status != s ) pthread_cond_wait( &cond_A_status, &mutex_A_status );
    pthread_mutex_unlock( &mutex_A_status );
}

/********************************************************************************
 * thread safe changing of the variable P_done
 ********************************************************************************/

void setPDone( int val )
{
    pthread_mutex_lock( &mutex_P_done );         /* get exclusive access to audio status variable */

    /*
     * ignore illegal values for P_done,
     * otherwise set P_done to requested value
     */
    if( val != 0 && val != 1 )
        fprintf( stderr, "Illegal value for P_done: %d\n", val );
    else
        P_done = val;

    pthread_mutex_unlock( &mutex_P_done );       /* give up exclusive access to audio status */
}

/********************************************************************************
 * thread safe reading of the variable P_done
 ********************************************************************************/

int getPDone(  )
{
    int retval;                                  /* return value */

    pthread_mutex_lock( &mutex_P_done );         /* get exclusive access to audio status variable */
    retval = P_done;                             /* put value of P_done in return variable */
    pthread_mutex_unlock( &mutex_P_done );       /* give up exclusive access to audio status */

    return retval;
}

/********************************************************************************
 * block audio recording thread until 'auto recording' is requested
 ********************************************************************************/

void waitForAutoRecordingRequest(  )
{
    /*
     * if semaphore is > 0, decrease it and continue,
     * otherwise the calling thread blocks here until semaphore > 0
     */
    semaphore_down( &auto_recording );
}

/********************************************************************************
 * main
 ********************************************************************************/

void usage( const char *prog )
{
    printf( "Version: " VERSION "\n" );
    printf( "Usage: %s [options] <speakermodel.cvc>\n", prog );
    printf( "Options:\n" );
    printf( "\t-o, --once     Run once, exit after first successfull recognition.\n" );
    printf( "\t               Exit code will be the id of recognized model.\n" );
    printf( "\t               This feature is provided for speech prompts in scripts.\n" );
    printf( "\t-d, --daemon   Run as daemon\n" );
    printf( "\t-v, --verbose  Verbose messages\n" );
    printf( "\t-V, --version  Print version and exit\n" );
    printf( "\t-h, --help     Show this help\n" );
    printf( "\n" );
}

int main( int argc, char *argv[] )
{
    /* thread variables */
    pthread_t record_t;
    pthread_t preprocess_t;
    pthread_t recognize_t;

    char *model_file;

    struct option long_options[] = {
        {"daemon", no_argument, 0, 'd'},
        {"once", no_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"version", no_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int ret;

    while( ( ret = getopt_long( argc, argv, "dovVh", long_options, NULL ) ) != -1 )
    {
        switch ( ret )
        {
            case 'o':
                run_once = 1;
                break;
            case 'v':
                g_verbose = 1;
                break;
            case 'd':
                g_daemon = 1;
                break;
            case 'V':
                printf( PACKAGE " version " VERSION "\n" );
                return 0;
            case 'h':
            default:
                usage( argv[0] );
                return 0;
        }
    }

    if( optind >= argc )
    {
        printf( "\nPlease specify speakermodel.cvc file!\n\n" );
        usage( argv[0] );
        return -1;
    }

    model_file = argv[optind];

    /*
     * load configuration from CONFIG_FILE:
     * see configuration.h for more information on what
     * configuration data is loaded ...
     */
    if( loadConfiguration(  ) == 0 )
    {
        fprintf( stderr, "Couldn't load configuration!" );
        fprintf( stderr, "Please run microphone_config before using this application!\n" );
        exit( -1 );
    }

    /* setup speaker model */

    model = ( Model * ) malloc( sizeof( Model ) );
    initModel( model );

    if( loadModel( model, model_file, 0 ) == 0 )
    {
        fprintf( stderr, "Failed to load speaker model: %s !\n", model_file );
        exit( -1 );
    }

    /*
     * initialize the two thread-safe queues that are
     * used to "connect" the three main threads
     */
    initQueue( &queue1, "char" );
    initQueue( &queue2, "float" );

    /* initialize mutex and semaphore variables */

    pthread_mutex_init( &mutex_A_status, NULL );
    pthread_cond_init( &cond_A_status, NULL );
    semaphore_init( &auto_recording );
    semaphore_down( &auto_recording );           /* init semaphore to '0' */

    pthread_mutex_init( &mutex_P_done, NULL );
    setPDone( 0 );

    /* create the three main threads */

    pthread_create( &record_t, NULL, ( void * )&record, NULL );
    pthread_create( &preprocess_t, NULL, ( void * )&preprocess, NULL );
    pthread_create( &recognize_t, NULL, ( void * )&recognize, NULL );

    waitForAudioStatus( A_off );
    setAudioStatus( A_prefetching );             /* for now: switch to auto recording after start up */

    /* join the threads here */

    pthread_join( record_t, NULL );
    pthread_join( preprocess_t, NULL );
    pthread_join( recognize_t, NULL );

    /* cleanup stuff */

    resetModel( model );
    free( model );

    if( run_once ) exit( result_id );            /* set exit code to the ID of the recognition result */
    else exit( 0 );                              /* leave program with exit code 'ok' */

}

/********************************************************************************
 * calculate euklidian distance of two feature vectors
 ********************************************************************************/

inline float euklid_distance( float *a, float *b )
{
    float result = 0;                            /* resulting distance */
    int i;
    /*
     * sum up the squares of the differences of the vector's components
     * the result is the square root of this value
     */
    for( i = 0; i < FEAT_VEC_SIZE; i++ ) result += ( a[i] - b[i] ) * ( a[i] - b[i] );
    result = sqrt( result );
    return result;
}

/********************************************************************************
 * recognizer thread
 ********************************************************************************/

void recognize( void )
{
    /*
     * frame is actual feature vector,
     * last_frame is feature vector at (pos-1)
     */
    float frame[FEAT_VEC_SIZE];
    float last_frame[FEAT_VEC_SIZE];

    /*
     * pos is the number of feature vectors
     * that have been processed
     */
    int pos = 0;

    /* initialize the status of the recognition thread to 'Q_invalid' */

    enum QStatus R_status = Q_invalid;

    /* loop variables */

    int i, j, k;

    /*
     * set to 1 if we are waiting for an 'abort'
     * to arrive through the queues
     */
    int abort_requested = 0;

    /*
     * counter variables for
     * a) list of references,
     * b) list of sample utterances
     */
    int ref, samp;

    /*
     * temporary pointer to a sample utterance
     * used during recognition
     */
    ModelItemSample *sample;

    float act_dist;                              /* (euklid) distance at current DTW position */
    float column_min_dist;                       /* minimum distance in current DTW column */
    float tmp_dist;                              /* temp. variable */

    /* branch&bound related stuff */

    /*
     * indicates whether recognition is done with B&B method
     */
    int do_branchNbound = 0;

    /*
     * position in test utterance where recognizer switched to B&B
     */
    int bNb_start_pos = 0;

    /*
     * buffer that holds the preprocessed data of a test utterance
     * when using B&B method, (plus length of utterance)
     */
    float **test_utterance = NULL;
    int test_utt_length = 0;

    /*
     * indicate whether recognition run has been finished
     * successfully
     */

    int recognition_done = 0;

    /*
     * queue that holds reference utterances sorted by their
     * recognition score so far.
     * used in B&B method
     */
    BBQueue bb_queue;
    initBBQueue( &bb_queue );

    /*
     * some recognizer-specific variables:
     * their meaning is described in server.h
     */

    /* adjust_win_width and score_threshold should be put in a config file type of place */
    adjust_window_width = 90;
    sloppy_corner = 4;
    /* score_threshold = 18; */
    float_max = FLT_MAX * 0.0001;

    /*
     * initialize score_queue:
     * this is a queue that holds a sorted (by score) list of recognition hypothesis
     * from which the final recognition result is derived
     */
    initScoreQueue( &score_queue );

    /* main loop of recognition thread */

    while( running )
    {
        /*
         * report results and reset score queue if recognition
         * finished successfully.
         */
        if( recognition_done )
        {
            int id = getResultID( &score_queue );

            recognition_done = 0;

            if( id >= 0 )                        /* something recognized! */
            {
                if( run_once || strcmp( ( getModelItem( model, id ) )->command,
                                        "cvoicecontrol_off" ) == 0 )
                {
                    result_id = id;

                    /* exit!! */
                    /*fprintf(stderr, "Exit ID: %d\n", id); */

                    setAudioStatus( A_exiting );
                    break;
                }
                else
                {
                    /* execute command */
                    /*fprintf(stderr, "%s\n", (getModelItem(model, id))->label); */
                    system( ( getModelItem( model, id ) )->command );
                }
            }

            /* free the space occupied by score_queue */
            resetScoreQueue( &score_queue );
            beep(  );
            setAudioStatus( A_prefetching );
        }

        /*
         * time-synchronous calculation of the DTW matrices of all reference utterances
         * at column 'pos'
         */
        if( !do_branchNbound )
        {
            float *tmp_data;

            /*
             * If an abort was requested by the last frame that was extracted
             * from the queue from the preprocessing thread,
             * we empty the queue until an 'end'- or an 'abort'-type frame
             * comes through the queue, then the recognizer is reset to
             * wait for the next utterance ...
             */
            if( abort_requested )
            {
                /*
                 * number of remaining frames in queue after
                 * the preprocessing of the current utterance has finished
                 */
                int remaining_frames;

                enum QStatus tmp_status;         /* temporary variable */

                int i;                           /* counter variable */

                /*
                 * wait until preprocessing has finished,
                 * then reset P_done to 0 ...
                 */
                while( !getPDone(  ) ) ;
                setPDone( 0 );

                /* ... and empty the queue */

                remaining_frames = numberOfElements( &queue2 );
                for( i = 0; i < remaining_frames; i++ ) dequeue( &queue2, &tmp_status );

                abort_requested = 0;             /* reset 'abort_requested' to 0 */

                /*  if negative recognition answer is desired, output one here!! */
                /*  implement any further abort functionality !!!!!? */

                waitForAudioStatus( A_off );
                beep(  );
                setAudioStatus( A_prefetching ); /* switch audio thread back to 'auto recording' */

                continue;
            }

            /*
             * get next feature vector from the head of queue2
             * last frame is moved to last_frame
             * (the dequeue function blocks the current thread while the queue is empty)
             */
            tmp_data = dequeue( &queue2, &R_status );
            memcpy( last_frame, frame, sizeof( float ) * FEAT_VEC_SIZE );
            memcpy( frame, tmp_data, sizeof( float ) * FEAT_VEC_SIZE );
            free( tmp_data );

            /*
             * check whether switching to B&B makes sense
             * if yes, prepare for and switch to B&B method
             *
             * this check is only performed if the first couple of DTW columns,
             * that need 'special treatment', have already been calculated AND
             * if preprocessing has already been finished
             * the latter condition is required as we need to know the actual length
             * of the currently incoming utterance to be able to use the B&B method!
             */
            if( getPDone(  ) && pos >= sloppy_corner + 1 )
            {
                /*
                 * remaining frames to evaluate with B&B  =  frames in the queue + last_frame + (current) frame !
                 */
                int remaining_frames = numberOfElements( &queue2 ) + 2;

                int i;
                /* counter variable */

                setPDone( 0 );
                /* reset P_done to 0 ... */

                /*
                 * if at least 30 frames are left to evaluate and abort has not been requested,
                 * switch to B&B method
                 */
                if( remaining_frames >= 30 && !abort_requested )
                {
                    test_utt_length = remaining_frames + pos;
                    /* total length of test utterance */

                    /* switch to branch and bound method at DTW column 'pos' */

                    do_branchNbound = 1;
                    bNb_start_pos = pos;

                    /* retrieve all remaining frames from queue and put them in an array */

                    test_utterance = ( float ** )malloc( sizeof( float ** ) * remaining_frames );
                    test_utterance[0] = last_frame;
                    test_utterance[1] = frame;
                    for( i = 2; i < remaining_frames; i++ )
                        test_utterance[i] = dequeue( &queue2, &R_status );

                    /*
                     * setup B&B queue:
                     * put all sample utterances in the queue sorted by increasing score,
                     * deactivate all utterances that don't meet the required constraints
                     * (this can be determined now, as the length of the test utterance is known at this point)
                     */
                    for( i = 0; i < model->total_number_of_sample_utterances; i++ )
                    {
                        int I = model->direct[i]->length;   /* length of sample utterance */
                        float tmp_dist = float_max, tmp_dist2;  /* temporary variables */

                        int j;

                        if( !( model->direct[i]->isActive ) ||  /* item still active */
                            ( ( test_utt_length - 1 ) * 2 + ( sloppy_corner - 1 ) < I - 1 ) ||
                            /* min slope is 0.5 */
                            ( ( test_utt_length - sloppy_corner ) / 2 > I - 1 ) ||
                            /* max slope is 2 */
                            ( I + adjust_window_width < test_utt_length ) ||
                            /* adjustment window right side */
                            ( I - adjust_window_width > test_utt_length ) )
                            /* adjustment window left side */
                            continue;

                        /* get minimum distance in actual column */

                        for( j = 0; j < I; j++ )
                        {
                            tmp_dist2 =
                                model->direct[i]->matrix[pos % 3][j] / ( ( pos + 1 ) + ( j + 1 ) );
                            if( tmp_dist2 < tmp_dist ) tmp_dist = tmp_dist2;
                        }

                        /* insert item into B&B queue */

                        insertIntoBBQueue( &bb_queue, pos + 1, tmp_dist, i );
                    }

                    continue;
                    /* enter next 'while' step in B&B mode */
                }
            }

            /* react to status of current queue item */

            switch ( R_status )
            {
                case Q_invalid:
                    /* this case should not occur! */
                    break;
                case Q_start:
                    /* start of a new utterance */
                    pos = 0;                     /* start at position 'pos' */
                    resetScoreQueue( &score_queue );
                    /* make sure, the score queue is empty */
                    activateAllSamples( model );
                    /* activate all model items! */
                    break;
                case Q_data:
                    /* data or end-type frame */
                case Q_end:
                    pos++;                       /* advance to next position in DTW matrix */
                    /* abort_requested = 0; */
                    break;
                case Q_abort:
                    /* start next while loop, abort is handled there */
                    continue;
                    break;
                case Q_exit:
                    /* exit program */
                    continue;
                    break;
            }

            /* loop all sample utterances */

            for( samp = 0; samp < model->total_number_of_sample_utterances; samp++ )
            {
                /* - skip if sample is inactive */

                if( !model->direct[samp]->isActive )
                    continue;

                /*
                 * ref and sample are pointers to the current reference item and
                 * sample utterance respectively, just to increase code clarity.
                 */
                ref = model->direct_map2ref[samp];
                sample = model->direct[samp];

                /*
                 * deactivate sample if it is too short to be aligned with the current
                 * adjust_window_width, if no more samples are left, request abort!
                 */
                if( pos - adjust_window_width > sample->length )
                {
                    model->direct[samp]->isActive = 0;
                    /* deactivate sample */

                    /* all samples deactivated!! request abort! */

                    model->number_of_active_sample_utterances--;
                    if( model->number_of_active_sample_utterances <= 0 )
                    {
                        abort_requested = 1;
                        setAudioStatus( A_aborting );   /*  what would happen if (audioStatus == A_off) at this point? */
                        break;
                        /* break for loop, -> enter next while loop */
                    }
                    else
                        continue;
                    /* sample deactivated, continue with next sample */
                }

                /*
                 * after all items in a column are calculated, this variable contains the minimum
                 * distance value in the current column
                 */
                column_min_dist = float_max;

                /* fill actual DTW matrix column */

                if( pos == 0 )
                    /* at pos == 0, we initialize the first matrix column */
                {
                    /* initialize current DTW matrix column, i.e. set all values to 'infinity' */

                    for( k = 0; k < sample->length; k++ )
                        sample->matrix[0][k] = float_max;

                    /* ccalculate the first <sloppy_corner> items in the current column */

                    sample->matrix[0][0] = 2 * euklid_distance( sample->data[0], frame );
                    column_min_dist = sample->matrix[0][0] / ( ( 0 + 1 ) + ( 0 + 1 ) );

                    for( i = 1; i < sloppy_corner; i++ )
                    {
                        sample->matrix[0][i] =
                            sample->matrix[0][i - 1] + euklid_distance( sample->data[i], frame );

                        tmp_dist = sample->matrix[0][i] / ( ( 0 + 1 ) + ( i + 1 ) );
                        if( tmp_dist < column_min_dist )
                            column_min_dist = tmp_dist;
                    }
                }
                else if( pos == 1 )
                {
                    /*
                     * at pos == 1, we use a special (shorter) warping function
                     * as the history (of one matrix column) does not allow
                     * for the application of the full warping function yet
                     */

                    /* initialize current DTW matrix column, i.e. set all values to 'infinity' */

                    for( k = 0; k < sample->length; k++ )
                        sample->matrix[1][k] = float_max;

                    /* calculate the first <sloppy_corner+1> elements */

                    sample->matrix[1][0] =
                        sample->matrix[0][0] + euklid_distance( sample->data[0], frame );
                    column_min_dist = sample->matrix[1][0] / ( ( 1 + 1 ) + ( 0 + 1 ) );

                    sample->matrix[1][1] =
                        MIN3( sample->matrix[0][1] + act_dist,
                              sample->matrix[1][0] + act_dist,
                              sample->matrix[0][0] + 2 * act_dist );

                    tmp_dist = sample->matrix[1][1] / ( ( 1 + 1 ) + ( 1 + 1 ) );
                    if( tmp_dist < column_min_dist )
                        column_min_dist = tmp_dist;

                    for( i = 2; i < sloppy_corner + 1; i++ )
                    {
                        act_dist = euklid_distance( sample->data[i], frame );

                        sample->matrix[1][i] =
                            MIN3( sample->matrix[0][i] + act_dist,
                                  sample->matrix[0][i - 1] + 2 * act_dist,
                                  sample->matrix[0][i - 2] +
                                  2 * euklid_distance( sample->data[i - 1], frame ) + act_dist );

                        tmp_dist = sample->matrix[1][i] / ( ( 1 + 1 ) + ( i + 1 ) );
                        if( tmp_dist < column_min_dist )
                            column_min_dist = tmp_dist;
                    }
                }
                else if( pos > 1 )
                {
                    /*
                     * beyond pos == 1, the warping function lies inside the matrix
                     * and can be calculated completely.
                     */

                    /* initialize current DTW matrix column, i.e. set all values to 'infinity' */

                    for( k = 0; k < sample->length; k++ )
                        sample->matrix[pos % 3][k] = float_max;

                    /* take care of sloppy start */

                    if( pos < sloppy_corner )
                        /* element in first row of DTW matrix */
                    {
                        sample->matrix[pos % 3][0] =
                            sample->matrix[( pos - 1 ) % 3][0] +
                            euklid_distance( sample->data[0], frame );
                        column_min_dist = sample->matrix[pos % 3][0] / ( ( pos + 1 ) + ( 0 + 1 ) );
                    }
                    if( pos < sloppy_corner + 1 )
                        /* element in second row of DTW matrix */
                    {
                        act_dist = euklid_distance( sample->data[1], frame );

                        /* use a simpler, smaller warping function that fits into the DTW matrix */

                        sample->matrix[pos % 3][1] =
                            MIN3( sample->matrix[pos % 3][0] + act_dist,
                                  sample->matrix[( pos - 1 ) % 3][0] +
                                  2 * act_dist,
                                  sample->matrix[( pos - 2 ) % 3][0] +
                                  2 * euklid_distance( sample->data[1], last_frame ) + act_dist );

                        tmp_dist = sample->matrix[pos % 3][1] / ( ( pos + 1 ) + ( 1 + 1 ) );
                        if( tmp_dist < column_min_dist )
                            column_min_dist = tmp_dist;
                    }

                    /*
                     * loop rows in current DTW column within
                     * - range of adjust_window,
                     * - range of warping function
                     * - sample length
                     */

                    for( j =
                         MAX3( 2, pos - adjust_window_width, ( pos - 2 ) / 2 );
                         j < MIN3( sloppy_corner + 1 + ( pos - 1 ) * 2,
                                   sample->length, pos + adjust_window_width ); j++ )
                    {
                        act_dist = euklid_distance( sample->data[j], frame );

                        /*
                         * apply warping function to calculate current DTW matrix element
                         * (see server.h for description of warping function)
                         */

                        if( sample->matrix[( pos - 1 ) % 3][j - 1] < float_max
                            || sample->matrix[( pos - 1 ) % 3][j - 2] <
                            float_max || sample->matrix[( pos - 2 ) % 3][j - 1] < float_max )
                        {
                            sample->matrix[pos % 3][j] =
                                MIN3( sample->matrix[( pos - 1 ) % 3][j - 1] +
                                      2 * act_dist,
                                      sample->matrix[( pos - 1 ) % 3][j - 2] +
                                      2 * euklid_distance( sample->data[j - 1],
                                                           frame ) + act_dist,
                                      sample->matrix[( pos - 2 ) % 3][j - 1] +
                                      2 * euklid_distance( sample->data[j],
                                                           last_frame ) + act_dist );

                            tmp_dist = sample->matrix[pos % 3][j] / ( ( pos + 1 ) + ( j + 1 ) );
                            if( tmp_dist < column_min_dist )
                                column_min_dist = tmp_dist;
                        }
                        else
                            sample->matrix[pos % 3][j] = float_max;
                    }

                    /*
                     * if the minimum distance in the current row exceeds the
                     * overall threshold, deactivate this sample utterance!
                     */

                    if( column_min_dist > score_threshold )
                    {
                        model->direct[samp]->isActive = 0;
                        /* deactivate sample */

                        /* all samples deactivated!! request abort! */

                        model->number_of_active_sample_utterances--;
                        if( model->number_of_active_sample_utterances <= 0 )
                        {
                            abort_requested = 1;
                            setAudioStatus( A_aborting );   /*  what would happen if (audioStatus == A_off) at this point? */
                            break;
                            /* break for loop, -> enter next while loop */
                        }
                        else
                            continue;
                    }

                    if( R_status == Q_end )
                    {
                        /* calculate score from upper right corner of DTW matrix */

                        int s;
                        float tmp_score =
                            sample->matrix[pos % 3][sample->length - 1] / ( pos + sample->length );
                        for( s = 1; s < sloppy_corner; s++ )
                        {
                            tmp_dist =
                                sample->matrix[pos % 3][sample->length - 1 -
                                                        s] / ( pos + sample->length - s );
                            if( tmp_dist < tmp_score )
                                tmp_score = tmp_dist;
                        }

                        /*
                         * if the final score is below the score_threshold
                         * enqueue the pair (utterance/score) into the ScoreQueue
                         *  (sorted by increasing recognition score)
                         */
                        if( tmp_score <= score_threshold )
                            insertInScoreQueue( &score_queue, tmp_score, ref );
                    }
                    /*  if (R_status == Q_end)  */
                }
                /*  else if (pos > 1) */

            }
            /*  for (samp ...) */

            /*
             * if all sample utterances have been processed at final position
             * retrieve recognition result from ScoreQueue
             */
            if( R_status == Q_end && !abort_requested )
            {
                /* ready for the next recording session */

                waitForAudioStatus( A_off );
                //fprintf(stderr, ".");

                recognition_done = 1;
            }
        }
        else                                     /* B&B mode! */
        {
            BBQueueItem *item = 0;

            int nbest = 6;                       /* find the 'nbest' best hypotheses using B&B the method */
            int nbest_found = 0;

            /* do B&B until 'nbest' hypotheses have been found or B&B queue is empty */

            while( nbest_found < nbest && bb_queue.length > 0 )
            {
                /* remove first element (having minimum score) from list */

                item = headBBQueue( &bb_queue );

                /*
                 * if (at right corner) -> best item found
                 * (increase N-best counter, if ==0, stop)
                 */
                if( item->pos == test_utt_length - 1 )
                {
                    /* insert results of current hypothesis into the score queue */

                    insertInScoreQueue( &score_queue, item->score,
                                        model->direct_map2ref[item->sample_index] );

                    nbest_found++;
                    free( item );
                }
                else
                {
                    /*
                     * else expand the DTW matrix calculation of
                     * the current sample utterance by one column ...
                     */
                    int pos = item->pos;
                    ModelItemSample *sample = model->direct[item->sample_index];
                    float column_min_dist = float_max;
                    float tmp_dist;
                    int bottom, top;

                    for( j = 0; j < sample->length; j++ )
                        sample->matrix[pos % 3][j] = float_max;

                    if( pos < test_utt_length - 1 )
                        /* on right edge, just evaluate sloppy corner items */
                    {
                        bottom = MAX3( 2, pos - adjust_window_width, ( pos - 2 ) / 2 );
                        top =
                            MIN3( sloppy_corner + 1 + ( pos - 1 ) * 2,
                                  sample->length, pos + adjust_window_width );
                    }
                    else
                        /* otherwise follow the general constraints within the DTW matrix */
                    {
                        bottom =
                            MAX3( sample->length - sloppy_corner,
                                  pos - adjust_window_width, ( pos - 2 ) / 2 );
                        top = sample->length;
                    }

                    /* calculate relevant entries in the DTW matrix */

                    for( j = bottom; j < top; j++ )
                    {
                        act_dist =
                            euklid_distance( sample->data[j], test_utterance[pos - bNb_start_pos] );

                        /*
                         * apply warping function to calculate current DTW matrix element
                         * (see server.h for a detailed description of the warping function)
                         */

                        if( sample->matrix[( pos - 1 ) % 3][j - 1] < float_max
                            || sample->matrix[( pos - 1 ) % 3][j - 2] <
                            float_max || sample->matrix[( pos - 2 ) % 3][j - 1] < float_max )
                        {
                            sample->matrix[pos % 3][j] =
                                MIN3( sample->matrix[( pos - 1 ) % 3][j - 1] +
                                      2 * act_dist,
                                      sample->matrix[( pos - 1 ) % 3][j - 2] +
                                      2 * euklid_distance( sample->data[j - 1],
                                                           test_utterance[pos -
                                                                          bNb_start_pos] )
                                      + act_dist,
                                      sample->matrix[( pos - 2 ) % 3][j - 1] +
                                      2 * euklid_distance( sample->data[j],
                                                           test_utterance[pos -
                                                                          bNb_start_pos
                                                                          - 1] ) + act_dist );

                            tmp_dist = sample->matrix[pos % 3][j] / ( ( pos + 1 ) + ( j + 1 ) );
                            if( tmp_dist < column_min_dist )
                                column_min_dist = tmp_dist;
                        }
                        else
                            sample->matrix[pos % 3][j] = float_max;
                    }

                    /* reinsert the item into the BBQueue if the score is still below the threshold */

                    if( column_min_dist <= score_threshold )
                    {
                        item->pos++;
                        item->score = column_min_dist;

                        insertItemIntoBBQueue( &bb_queue, item );
                    }
                }
            }

            /* ready for next recording session */
            waitForAudioStatus( A_off );
            //fprintf(stderr, ",");

            /* B&B search done, report results */

            resetBBQueue( &bb_queue );
            /* reset B&B related variables */
            for( i = 2; i < test_utt_length - bNb_start_pos; i++ )
                free( test_utterance[i] );
            free( test_utterance );
            test_utterance = NULL;
            do_branchNbound = 0;

            recognition_done = 1;
        }
    }
}

/********************************************************************************
 * preprocessing thread
 ********************************************************************************/

void preprocess( void )
{
    /* data container used for fft calculation */
    float frame[FFT_SIZE];

    /* holds current buffer of waveform data plus remainders of last buffer */
    unsigned char buffer[FRAG_SIZE + OFFSET];

    /* amount of valid chars in buffer, 0 .. frag_size+offset */
    int buffer_counter = 0;

    /* number of whole frames that can be extracted from current waveform buffer */
    int frames_N;

    /* counter variables */
    int frameI, i;

    /* remainder  */
    int remainder;

    /* status of current queue item */
    enum QStatus P_status;

    /* init melscale buffer ... */
    float feat_vector[FEAT_VEC_SIZE];

    /* initialize preprocessing */
    initPreprocess(  );

    /* main loop of the preprocessing thread */
    while( running )
    {
        /*
         * get head data and status from queue1
         * (the dequeue function blocks the preprocessing thread if the queue is empty!)
         */
        char *tmp_data = ( char * )dequeue( &queue1, &P_status );
        memcpy( buffer + buffer_counter, tmp_data, FRAG_SIZE );
        free( tmp_data );

        /* react to status of current queue item */
        switch ( P_status )
        {
            case Q_invalid:
                /* this case should not occur! */
                break;

            case Q_start:
                /* start of a new utterance */
                buffer_counter = 0;
                /* set buffer_counter to the beginning of the buffer */
                while( getPDone(  ) != 0 ) setPDone( 0 );
                /* make sure P_done is set to 0 */
                break;

            case Q_data:
                /* nothing special to do at this point, just process data below */
            case Q_end:
                break;

            case Q_abort:
                /* don't process this frame */
                /* insert an 'abort' marked frame into the queue to signal 'aborting'! */
                enqueue( &queue2, feat_vector, FEAT_VEC_SIZE, Q_abort );
                setPDone( 1 );
                continue;                        /* skip the current step of the while loop */
                break;

            case Q_exit:
                enqueue( &queue2, feat_vector, FEAT_VEC_SIZE, Q_exit );
                continue;
                break;
        }

        /* make the buffer_counter point behind the currently received data */
        buffer_counter += FRAG_SIZE;

        /*
         * number of frames that can be extracted from the current amount
         * of available audio data
         */
        frames_N = ( buffer_counter - FFT_SIZE_CHAR ) / OFFSET + 1;

        /* extract these frames: */

        for( frameI = 0; frameI < frames_N; frameI++ )
        {
            /* prepare a hamming windowed frame from the audio data ... */

            for( i = 0; i < FFT_SIZE; i++ )
                frame[i] =
                    ( ( float )
                      ( ( signed short )( buffer[frameI * OFFSET + 2 * i] |
                                          ( buffer[frameI * OFFSET + 2 * i + 1]
                                            << 8 ) ) ) ) * hamming_window[i];

            preprocessFrame( frame, feat_vector );  /* ... and have it preprocessed */

            /* put the resulting feature vector in queue2 using the proper status flag */

            if( P_status == Q_start )
            {
                enqueue( &queue2, feat_vector, FEAT_VEC_SIZE, Q_start );
                //fprintf(stderr, "Start preprocessing!\n");
                P_status = Q_data;
            }
            else if( P_status == Q_end && frameI == frames_N - 1 )
            {
                enqueue( &queue2, feat_vector, FEAT_VEC_SIZE, Q_end );
                setPDone( 1 );
                //fprintf(stderr, "Done preprocessing!\n");
            }
            else
            {
                enqueue( &queue2, feat_vector, FEAT_VEC_SIZE, Q_data );
            }
        }

        /* there may be remaining data that did not fit in the last frame */
        remainder = buffer_counter - frames_N * OFFSET;

        /*
         * move this remaining data to the front of the buffer and adjust the buffer_counter
         *
         * use memcpy, if the two memory regions do not overlap, otherwise use for
         */
        if( remainder < frames_N * OFFSET )
            memcpy( buffer, buffer + frames_N * OFFSET, remainder );
        else
            for( i = 0; i < remainder; i++ )
                buffer[i] = buffer[i + frames_N * OFFSET];

        buffer_counter = remainder;
    }

    endPreprocess(  );
    /* free any allocated memory etc. */
}

/********************************************************************************
 * audio recording thread
 ********************************************************************************/

void record( void )
{
    /*
     * number of incoming blocks that need to be
     * considered '[not] containing speech' before recording is started
     */
    int count = 0;

    /* set prefetch buffer size to 5, and allocate the required memory */

    int prefetch_N = 5;
    int prefetch_pos = 0;
    unsigned char prefetch_buf[FRAG_SIZE * prefetch_N];
    void *prefetch = prefetch_buf;

    /*
     * a buffer of size 'FRAG_SIZE' that contains (raw) audio data which
     * was recorded from the sound card.
     */
    unsigned char buffer_raw[FRAG_SIZE];

    int new_start = 1;                           /* first if statement inside while loop initializes the recording */

    struct audio_buf_info info;
    signed short max = 0;
    signed short value;
    int i;

    /* main loop of the audio recording thread */

    while( running )
    {
        if( getAudioStatus(  ) == A_exiting )
        {
            running = 0;
            /* enqueue an 'abort'-type frame into queue1 */
            enqueue( &queue1, buffer_raw, FRAG_SIZE, Q_exit );
            break;
        }

        if( getAudioStatus(  ) == A_aborting )
        {
            /*
             * got message from recognition thread that the current recording
             * wouldn't match any of the hypotheses, -> abort!
             */

            /* enqueue an 'abort'-type frame into queue1 */
            enqueue( &queue1, buffer_raw, FRAG_SIZE, Q_abort );

            /* wait for audio signal to fall back to silence!! */

            for( count = 0; count < CONSECUTIVE_NONSPEECH_BLOCKS_THRESHOLD; )
            {
                if( readAudio( buffer_raw, FRAG_SIZE ) < 0 )
                {
                    fprintf( stderr, "audio device read error!\n" );
                    exit( -1 );
                }

                max = 0;
                for( i = 0; i < FRAG_SIZE / 2 - 1; i += 2 )
                {
                    value = abs( ( signed short )( buffer_raw[i] | ( buffer_raw[i + 1] << 8 ) ) );
                    if( value > max ) max = value;
                }

                //fprintf(stderr, "WAITING MAX: %d\n", max);

                if( max <= stop_level ) count++;
                else count = 0;
            }

            new_start = 1;
        }

        if( new_start )
        {
            closeAudio(  );
            count = 0;                           /* reset count */
            prefetch_pos = 0;                    /* reset position in audio prefetch buffer */
            setAudioStatus( A_off );             /* set status to A_off */
            memset( prefetch_buf, 0x00, FRAG_SIZE * prefetch_N );

            /* wait at a semaphore for 'auto recording' request */
            waitForAutoRecordingRequest(  );

            /*
             * actually, opening the audio device here
             * should be repeated a specified number of times
             * in case it fails, (perhaps with a somewhat more
             * professional delay than just using a for-loop ;-) )
             * if opening fails a couple of times, turn off
             * auto recording!!!!!
             */

            /* connect to microphone */
            while( openAudio(  ) == AUDIO_ERR )
            {
                if( !running ) break;
                fprintf( stderr, "AUDIO_ERR\n" );
                sleep( 10 );
            }

            new_start = 0;
        }

        /* use 'select()' to connect to the audio device */

        /* ... read the data from the device */
        if( readAudio( buffer_raw, FRAG_SIZE ) < 0 )
        {
            fprintf( stderr, "audio device read error!\n" );
            exit( -1 );
        }

        if( getAudioStatus(  ) == A_prefetching )
        {
            /* prefetch data into a circular buffer ... */

            memcpy( prefetch + prefetch_pos * ( FRAG_SIZE ), buffer_raw, FRAG_SIZE );
            prefetch_pos = ( prefetch_pos + 1 ) % prefetch_N;

            /* and check it for speech content */

            max = 0;
            for( i = 0; i < FRAG_SIZE / 2 - 1; i += 2 )
            {
                value = abs( ( signed short )( buffer_raw[i] | ( buffer_raw[i + 1] << 8 ) ) );
                if( value > max ) max = value;
            }

            //fprintf(stderr, "MAX: %d\n", max);

            if( max >= rec_level ) count++;
            else count = 0;

            if( count >= CONSECUTIVE_SPEECH_BLOCKS_THRESHOLD )  /* if speech detected ... */
            {
                count = 0;

                //fprintf(stderr, "Recording\n");

                /* ... start recording, ...  */

                setAudioStatus( A_recording );

                /* ... extract the data from the prefetch buffer and insert it into queue1 */

                for( i = prefetch_pos; i < prefetch_pos + prefetch_N; i++ )
                    enqueue( &queue1,
                             prefetch + ( i % prefetch_N ) * FRAG_SIZE,
                             FRAG_SIZE, ( i == prefetch_pos ? Q_start : Q_data ) );
            }
        }
        else if( getAudioStatus(  ) == A_recording )    /* currently recording audio data ... */
        {
            /* check whether no more speech signal, then stop recording */

            max = 0;
            for( i = 0; i < FRAG_SIZE / 2 - 1; i += 2 )
            {
                value = abs( ( signed short )( buffer_raw[i] | ( buffer_raw[i + 1] << 8 ) ) );
                if( value > max ) max = value;
            }

            //fprintf(stderr, "MAX: %d\n", max);

            if( max <= stop_level ) count++;
            else count = 0;

            /*
             * recording will be stopped,
             * if more than a predefined number of non-speech blocks come in a row
             */

            if( count >= CONSECUTIVE_NONSPEECH_BLOCKS_THRESHOLD )
            {
                /* here we insert the last data chunk into queue1 */
                enqueue( &queue1, buffer_raw, FRAG_SIZE, Q_end );

                count = 0;
                /* reset count */

                /* turn off recognition */
                //fprintf(stderr, "Stopped\n");

                new_start = 1;
            }
            else
            {
                /* insert the current chunk of data into queue1 */
                enqueue( &queue1, buffer_raw, FRAG_SIZE, Q_data );
            }
        }
    }
}
