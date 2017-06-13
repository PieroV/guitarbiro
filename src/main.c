#include "gui.h"

/// fprintf
#include <stdio.h>

#include <gtk/gtk.h>

int main(int argc, char *argv[])
{
	GUIContext *ctx;

	gtk_init(&argc, &argv);

	if(!(ctx = guiInitMain())) {
		fprintf(stderr, "Errors occurred while trying to initilize the GUI.\n");
		return 1;
	}

	gtk_main();

	guiFree(ctx);
	ctx = 0;

	return 0;
}
