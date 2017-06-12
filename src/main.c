#include "gui.h"

/// fprintf
#include <stdio.h>

#include <gtk/gtk.h>

int main(int argc, char *argv[])
{
	gtk_init(&argc, &argv);

	if(!guiInitMain()) {
		fprintf(stderr, "Errors occurred while trying to initilize the GUI.\n");
	}

	gtk_main();

	return 0;
}
