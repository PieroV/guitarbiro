/**
 * @file gui.h
 * @brief Manages graphical user interfaces
 *
 * This file aims to provide an API to the rest of the program to manage the GUI
 * without having to bother about parameters to pass or contexts.
 * It should be as easy and direct as possible.
 */

#ifndef __GUI_H
#define __GUI_H

#include "guitar.h"

/**
 * @brief Initializes the main window
 *
 * @return Did the operation succeed?
 */
extern int guiInitMain();

/**
 * @brief Set the frets that must be highlighted in the guitar neck.
 *
 * @note This function is not thread safe
 * @param frets An array of GUITAR_STRINGS elements that have for each string
 *  the fret that has to be highlighted, or a negative number if no fret has to
 *  be highlighted for that string
 */
extern void guiHighlightFrets(semitone_t *frets);

/**
 * @brief Remove highlights from the guitar neck
 *
 * @note This function is not thread safe
 * @sa guiHighlightFrets
 */
extern void guiResetHighlights();

#endif /* __GUI_H */
