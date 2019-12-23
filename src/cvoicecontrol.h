/***************************************************************************
                          cvoicecontrol.h  -  a simple speech recognizer
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

#ifndef CVOICECONTROL_H
#define CVOICECONTROL_H

/********************************************************************************
 * Symmetric Sahoe&Chiba warping function:
 ********************************************************************************
 *
 *
 *     |-------|-------|-------|-------|
 *     |       |       |       |       |   The element x in the DTW matrix
 *     |       |       |       |       |   is calculated as the minimum of
 *     |       |       |       |       |   the three paths is shown in the
 *     |-------|-------|-------|-------|   diagram to the left:
 *     |i-2/j  |i-1/j  |i/j    |       |
 *     |       |   o-------x   |       |    P1: M[i-2][j-1] +
 *     |      P1__/    |__/|   |       |         2 x Dist(sample[j], last_frame) +
 *     |-----__/-----__/---|---|-------|         Dist(sample[j], frame)
 *     |    /  |    /P2|   |   |       |
 *     |   o   |   o   |   o   |       |    P2: M[i-1][j-1] +
 *     |       |       |__/    |       |         2 x Dist(sample[j], frame)
 *     |-------|-----__/P3-----|-------|
 *     |       |    /  |       |       |    P3: M[i-1][j-2] +
 *     |       |   o   |       |       |         2 x Dist(sample[j-1], frame) +
 *     |       |       |i/j-2  |       |         Dist(sample[j], frame)
 *     |-------|-------|-------|-------|
 *
 */

/********************************************************************************
 * recognizer variables
 ********************************************************************************
 *
 *
 * width of the diagonal window in the DTW matrix that the
 *  evaluation is limited to.
 * This is used to reduce the number of DTW matrix elements
 *  that need to be calculated
 *            _________________________
 *           |                  *     .|    \
 *           |               *     .   |     |
 *           |            *     .     *|     |
 *           |         *<-w->.     *   |     |
 * reference |      *     .     *      |      > DTW-Matrix
 *           |   *     .     *         |     |
 *           |*     .<-w->*            |     |
 *           |   .     *               |     |
 *           |._____*__________________|    /
 *                    sample
 *
 *       Legend:  w = adjust_window_width
 */
int adjust_window_width;

/*
 * strictly speaking, time-alignment in the DTW matrix starts
 *  in the bottom left corner.
 * This value allows alignment to start up to `sloppy_corner`
 *  elements away from the corner
 *            _____ ... ____            _____ ... ____
 *          r|o             |          |o             |    \
 *          e|o             |          |o             |     |
 *          f|o             |          |o             |     |
 *          e|o             | -------> |o             |     |
 * sloppy_  r|o     ...     | sloppy_  |o     ...     |      > DTW-Matrix
 *  corner  e|o             |  corner  |o             |     |
 *  = 0     n|o             |  = 3     |x             |     |
 *          c|o             |          |x             |     |
 *          e|xoooo ... oooo|          |xxxoo ... oooo|    /
 *                sample                    sample
 *
 *    Legend: x = distance value,  o = (infinity)
 */
int sloppy_corner;

/*****
  time alignment scores must stay below this value
  otherwise the according samples are ignored in
  the further evaluation process
  *****/
float score_threshold;

/*****
  a (very high) float value that is considered "infinity"
  *****/
float float_max;

/***** these macros are used in the DTW warping function
  to calculate the maximum/minimum of two or three
  variables
  *****/
#define MAX2(a,b) ((a>b)?(a):(b))
#define MAX3(a,b,c) ((a>b)?(MAX2(a,c)):(MAX2(b,c)))
#define MIN2(a,b) ((a<b)?(a):(b))
#define MIN3(a,b,c) ((a<b)?(MIN2(a,c)):(MIN2(b,c)))

/********************************************************************************
 * preprocessing variables
 ********************************************************************************/

/* (none) */

/********************************************************************************
 * recording variables
 ********************************************************************************
 *
 * Recording is started when the micro level exceeds a
 * certain threshold.
 * some phones (like 's') produce a relatively small level
 * and are therefore likely to be cut from the beginning of
 * an utterance.
 * To overcome this, the following strategy is used:
 * in a circular 'prefetch' buffer incoming chunks of audio
 * data are stored constantly:
 *
 *      _____     _____     _____     _____     _____
 *     |     |   |     |   |     |   |     |   |     |
 *     |  1  |-->|  2  |-->|  3  |-->|  4  |-->|  5  |--
 *     |_____|   |_____|   |_____|   |_____|   |_____|  |
 *       / \                                            |
 *        |_____________________________________________|
 *
 *
 * The first junk is put in box '0', the next junk in box '1'
 * ... the fifth junk is put in box '4', the sixth junk is
 * put in box '0', generally the n-th junk is put in
 * box 'n modulo 5'
 * As a result, this circular buffer always contains the five
 * last chunks of the acoustic history.
 * When the actual recording is started, we can extract these
 * five chunks from the buffer and prepend them (in the proper)
 * order) to the recorded data.
 *
 * prefetch        circular buffer
 * prefetch_N      number of boxes
 * prefetch_pos    current position/box in buffer
 */

#endif
