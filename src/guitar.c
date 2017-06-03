/**
 * @file guitar.c
 * @brief Utilities for operations on musical notes and on guitar "structure".
 */

#include "guitar.h"

/// log2, roundf, pow
#include <math.h>

#include <stdio.h>

const semitone_t STANDARD_TUNING[GUITAR_STRINGS] = {
   43,	// E4
   38,	// B3
   34,	// G3
   29,	// D3
   24,	// A2
   19	// E2
};

/**
 * @brief The frequency of A0.
 */
static const double A0 = 27.5;

/**
 * @brief Interval between notes in semitones.
 *
 * The conversion from the name and octave of a note to semitones from A0 would
 * need operations of string, but since names actually are made of a letter from
 * A to G and eventually of a diesis or a bemolle, the conversion is handled
 * using this array.
 *
 * Its values are the semitones from A of each note: element 0 is A, element 1
 * is B and so on.
 */
static const semitone_t NOTE_INTERVALS[] = {0, 2, -9, -7, -5, -4, -2};

semitone_t noteToSemitones(const char *name, semitone_t octave)
{
	/// The value that will be returned
	semitone_t ret = octave * 12;

	/// The offset at which we expect to find \0
	int nullChar = 1;

	if(!((name[0] >= 'A' && name[0] <= 'G') ||
			(name[0] >= 'a' && name[0] <= 'g'))) {
		return INVALID_SEMITONE;
	}

	if(name[0] >= 'A') {
		ret += NOTE_INTERVALS[name[0] - 'A'];
	} else {
		// Will be lower case because of the previous if
		ret += NOTE_INTERVALS[name[0] - 'a'];
	}

	if(name[1] == 'b') {
		ret--;
		nullChar++;
	} else if(name[1] == '#') {
		ret++;
		nullChar++;
	}

	if(name[nullChar]) {
		return INVALID_SEMITONE;
	} else {
		return ret;
	}
}

double noteToFrequency(const char *name, semitone_t octave)
{
	semitone_t semitones = noteToSemitones(name, octave);

	if(semitones == INVALID_SEMITONE) {
		return -1.0;
	}

	return A0 * pow(2, semitones / 12.0);
}

semitone_t frequencyToSemitones(double frequency, double *error)
{
	/* This if uses <= instead of an epsilon check because actually logarithm
	has problems only with the exact 0 value, whereas with very low values
	(less than 10^-30) still returns acceptable values. */
	if(frequency <= 0.0) {
		return INVALID_SEMITONE;
	}

	semitone_t ret = (semitone_t)round(log2f(frequency / A0) * 12);

	if(error) {
		*error = A0 * pow(2, ret / 12.0) / frequency;
	}

	return ret;
}

unsigned int noteToFrets(semitone_t note, const semitone_t *tuning,
		semitone_t *frets, unsigned int strings, unsigned int fretsNumber)
{
	unsigned int valid = 0;

	for(unsigned int i = 0; i < strings; i++) {
		frets[i] = note - tuning[i];

		if(frets[i] > fretsNumber) {
			frets[i] = -1;
		} else if(frets[i] >= 0) {
			valid++;
		}
	}

	return valid;
}
