/**
 * @file main.c
 * @brief Manages the flow of the program
 *
 * This is the main file of the program. It contains the entry point and from
 * there initializes and runs all the other parts of it.
 */

/// All audio related functions
#include "record.h"

/**
 * @brief The entry point of the program
 *
 * @param argc The number of arguments received
 * @param argv The arguments
 * @return The status code to pass to the OS at the exit.
 */
int main(int argc, char *argv[])
{
	audioInit();
	audioRecord("/dev/shm/prova");

	return 0;
}
