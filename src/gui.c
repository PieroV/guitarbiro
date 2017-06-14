#include "gui.h"

/// fprintf, snprintf
#include <stdio.h>
/// malloc, free
#include <stdlib.h>

/// All the needed GTK functions
#include <gtk/gtk.h>
/// API to render SVG images, used to draw the guitar neck
#include <librsvg/rsvg.h>
/// Portable threading API
#include <glib.h>

/**
 * @brief The path to the XML file that describes the GUI
 */
static const char *GUI_FILE = "resources/main_window.glade";

/**
 * @brief The path to the guitar neck SVG file
 *
 * This file contains the figure of a guitar and a dot in each fret of each
 * string.
 * The neck is always shown, whereas dots are shown only when that note is
 * played.
 */
static const char *GUITAR_NECK_FILE = "resources/guitar_neck.svg";

/**
 * @brief The name of the layar that contains the neck of the guitar in the SVG
 *
 * To show only the neck, it has been drawn in its own layer, and this is its
 * name.
 */
static const char *GUITAR_NECK_LAYER = "#layer1";

/**
 * @brief The template to get the ID of dots in the guitar neck
 *
 * Each dot has its own id, which contains both the string number (from 1 to 6)
 * and the fret number (from 0 = open string to 22).
 * This string is intended to be used with sprintf.
 * It contains two integer placeholders: the former is used for the string
 * number, the latter for the fret number.
 */
static const char *GUITAR_DOTS_TEMPLATE = "#dot_%d_%d";

/**
 * @brief The size to alloc for dots id
 * @note It must include the space for the null byte
 */
#define GUITAR_DOTS_MAXLENGTH 12

/**
 * @brief Print an error on standard error if a pointer is null and return 0
 */
#define CHECK_WIDGET(ptr, name) if(!(ptr)) { \
		fprintf(stderr, "Could not find the %s.\n", name); \
		return 0; \
	}

struct _GUIContext {
	/// The main window of the program
	GtkWidget *mainWindow;

	/// A pointer to the settings window
	GtkWidget *settingsWindow;

	/// The button to start and stop recording
	GtkWidget *recordButton;

	/// The apply button on the settings dialog
	GtkWidget *applyButton;

	/// The cancel button on the settings dialog
	GtkWidget *cancelButton;

	/// A pointer to the list of backends of the settings dialog
	GtkComboBoxText *backendList;

	/// A pointer to the list of devices of the settings dialog
	GtkComboBoxText *deviceList;

	/// The AudioContext instance
	AudioContext *audio;

	/// The flag to stop the recording loop
	char keepRecording;

	/// The recording thread
	GThread *recordingThread;
};

/**
 * @brief Populate the GUI context taking from the builder the required widgets
 *
 * @param ctx An instance of the GUI context
 * @param builder The builder of the GUI
 * @param audio The instance of the audio context
 * @return Did the operation succeed?
 */
static int populateContext(GUIContext *ctx, GtkBuilder *builder,
		AudioContext *audio);

/**
 * @brief Connects the needed signals to GTK.
 *
 * @param ctx An instance of the GUI context
 * @param builder The builder of the GUI
 */
static void connectSignals(GUIContext *ctx, GtkBuilder *builder);

/**
 * @brief Insert backends and devices into settings lists.
 *
 * @param ctx An instance of the GUI context
 */
static void populateSettings(GUIContext *ctx);

/**
 * @brief Handle the window close signal
 */
static void windowClose();

/**
 * @brief Handle the start recording signal
 *
 * @param button The button that triggered the event
 * @param contextPtr A pointer to the GUI context instance
 */
static void recordClicked(GtkButton *button, gpointer contextPtr);

/**
 * @brief Callback function that just queues the redraw of the neck drawing area
 *
 * GTK isn't really thread safe, however it provides a way to get updates from
 * other threads, using the gdk_threads_add_idle function, which needs a
 * callback that will do operations on GTK objects.
 * In our case, queueRedraw is the callback, and just queues the redraw for the
 * drawing area that contains the guitar neck.
 *
 * Note that the function that wants to queue the redraw has to change gFrets
 * according to the changes it needs.
 * The userData parameter could have been used, but memory leaks prevention
 * would have been more difficult than using the global variable.
 *
 * @param userData A compulsory but not used parameter
 * @return Always return G_SOURCE_REMOVE to run this function once
 */
static gboolean queueRedraw(gpointer userData);

/**
 * @brief Handle redraw events
 *
 * @param widget The widget that triggered the event
 * @param cr The cairo context where the image will be rendered
 * @param userData Compulsory but not used parameter
 */
static void drawGuitar(GtkWidget *widget, cairo_t *cr, gpointer userData);

/**
 * @brief Callback to quit using the file menu
 *
 * @param menuItem The item that triggered the event
 * @param settingsWindow The pointer to the main window
 */
static void menuQuit(GtkMenuItem *menuItem, gpointer mainWindow);

/**
 * @brief Callback to open the preferences window.
 *
 * @param menuItem The item that triggered the event
 * @param settingsWindow The pointer to the preferences window
 */
static void menuShowSettings(GtkMenuItem *menuItem, gpointer settingsWindow);

/**
 * @brief Callback that handles the pression of the apply button on settings.
 *
 * @param button The button that triggered the event
 * @param contextPtr A pointer to the GUIContext instance
 */
static void settingsApply(GtkButton *button, gpointer contextPtr);

/**
 * @brief Callback that handles the pression of the cancel button on settings.
 *
 * @param contextPtr A pointer to the GUIContext instance
 */
static void settingsCancel(gpointer contextPtr);

/**
 * @brief Callback that handles changes on the list of backends in settings.
 *
 * @param backendList The list that triggered the event
 * @param contextPtr A pointer to the GUIContext instance
 */
static void settingsBackendChanged(GtkComboBox *backendList,
		gpointer contextPtr);

/**
 * @brief Prevent the settings dialog deletion.
 *
 * When a dialog is closed using the X button or the ESCAPE key, GTK
 * automatically triggers a delete event and it destroys the widget.
 * However this is not the behaviour we want from the program, because we want
 * to be able to show any time we want the settings dialog.
 *
 * @param widget The widget that triggers the event
 * @param event Information about the event
 * @param data Compulsory but not used parameter
 * @return TRUE, to mark the delete event as completed
 */
static gboolean windowPreventDelete(GtkWidget *widget, GdkEvent *event,
		gpointer data);

/**
 * @brief Start the recording thread
 *
 * @param ctx A pointer to the GUIContext
 */
static void startRecording(GUIContext *ctx);

/**
 * @brief Stop the recording thread
 * @note This will cause the thread to join, so it could take some time before
 *  returning
 *
 * @param ctx A pointer to the GUIContext
 */
static void stopRecording(GUIContext *ctx);

/**
 * @brief Recording thread callback.
 *
 * The data acquisition has been implemented in a blocking fashion, therefore it
 * needs its own thread, which, in this way, can also do the computations on the
 * signal, without having to bother about other things, such as GUI
 * responsiveness etc...
 *
 * The thread managing is done here to immeediately interact with GUI when we
 * have updates, instead of using more signals or similar things.
 *
 * @param contextPtr A pointer to the GUIContext instance
 * @return Always NULL
 */
static gpointer recordingWorker(gpointer contextPtr);

/**
 * @brief The array that contains the frets to highlight
 *
 * We use a global variable because we want to keep the API simple for other
 * parts of the program, and because the GTK API to use parameters on singals is
 * very pedantic.
 *
 * Note that changes on this variable aren't synchronized with mutex or similar,
 * however the GUI thread only reads it, and there should be only another thread
 * that manages the data acquisition and freqeuency detection, that writes this
 * variable, so actually the synchronization shouldn't be needed.
 */
static short gFrets[GUITAR_STRINGS] = {-1, -1, -1, -1, -1, -1};

/**
 * @brief The drawing area where the guitar neck is drawn.
 *
 * Since we want to offer an API that can be called by any part of the program,
 * we don't have any other way to share this pointer between functions.
 */
static GtkWidget *gDrawArea;

GUIContext *guiInitMain(AudioContext *audio)
{
	GUIContext *ctx;
	GtkBuilder *builder;

	ctx = malloc(sizeof(GUIContext));
	if(!ctx) {
		fprintf(stderr, "Could not allocate the GUIContext.\n");
		return 0;
	}

	builder = gtk_builder_new_from_file(GUI_FILE);
	if(!builder) {
		fprintf(stderr, "Could not create the GTK builder.\n");
		free(ctx);
		return 0;
	}

	if(!populateContext(ctx, builder, audio)) {
		// Errors in this case will be reported by populateContext
		g_object_unref(builder);
		free(ctx);
		return 0;
	}

	connectSignals(ctx, builder);
	populateSettings(ctx);

	g_object_unref(builder);

	gtk_widget_show(ctx->mainWindow);

	return ctx;
}

void guiFree(GUIContext *context)
{
	// Just need to free the structure, because GTK will free anything else
	free(context);
}

void guiHighlightFrets(semitone_t *frets)
{
	for(int i = 0; i < GUITAR_STRINGS; i++) {
		gFrets[i] = frets[i];
	}

	gdk_threads_add_idle(queueRedraw, NULL);
}

void guiResetHighlights()
{
	for(int i = 0; i < GUITAR_STRINGS; i++) {
		gFrets[i] = -1;
	}

	gdk_threads_add_idle(queueRedraw, NULL);
}

int populateContext(GUIContext *ctx, GtkBuilder *builder, AudioContext *audio)
{
	ctx->mainWindow = GTK_WIDGET(gtk_builder_get_object(builder, "MainWindow"));
	CHECK_WIDGET(ctx->mainWindow, "main window")

	ctx->settingsWindow = GTK_WIDGET(gtk_builder_get_object(builder,
			"SettingsWindow"));
	CHECK_WIDGET(ctx->settingsWindow, "settings window")

	ctx->recordButton = GTK_WIDGET(gtk_builder_get_object(builder, "record"));
	CHECK_WIDGET(ctx->recordButton, "button to start and stop recording")

	ctx->applyButton = GTK_WIDGET(gtk_builder_get_object(builder,
			"settingsApply"));
	CHECK_WIDGET(ctx->applyButton, "button to save settings")

	ctx->cancelButton = GTK_WIDGET(gtk_builder_get_object(builder,
			"settingsCancel"));
	CHECK_WIDGET(ctx->cancelButton, "button to cancel settings save")

	ctx->backendList = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder,
			"backendsList"));
	CHECK_WIDGET(ctx->backendList, "list of backends (in settings dialog)")

	ctx->deviceList = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder,
			"devicesList"));
	CHECK_WIDGET(ctx->deviceList, "list of devices (in settings dialog)")

	// The only exception that actually is not stored on the context
	gDrawArea = GTK_WIDGET(gtk_builder_get_object(builder, "NeckArea"));
	CHECK_WIDGET(gDrawArea, "guitar neck drawing area")

	ctx->audio = audio;

	// Until record doesn't start, keep this to 0
	ctx->keepRecording = 0;

	ctx->recordingThread = 0;

	return 1;
}

void connectSignals(GUIContext *ctx, GtkBuilder *builder)
{
	gtk_builder_add_callback_symbol(builder, "windowClose",
			G_CALLBACK(windowClose));
	gtk_builder_add_callback_symbol(builder, "drawGuitar",
			G_CALLBACK(drawGuitar));
	gtk_builder_add_callback_symbol(builder, "menuQuit",
			G_CALLBACK(menuQuit));
	gtk_builder_add_callback_symbol(builder, "menuShowSettings",
			G_CALLBACK(menuShowSettings));
	gtk_builder_connect_signals(builder, NULL);

	g_signal_connect_swapped(G_OBJECT(ctx->settingsWindow), "close",
			G_CALLBACK(settingsCancel), ctx);
	g_signal_connect(G_OBJECT(ctx->settingsWindow), "delete-event",
			G_CALLBACK(windowPreventDelete), NULL);
	g_signal_connect(G_OBJECT(ctx->recordButton), "clicked",
			G_CALLBACK(recordClicked), ctx);
	g_signal_connect(G_OBJECT(ctx->applyButton), "clicked",
			G_CALLBACK(settingsApply), ctx);
	g_signal_connect_swapped(G_OBJECT(ctx->cancelButton), "clicked",
			G_CALLBACK(settingsCancel), ctx);
	g_signal_connect(G_OBJECT(ctx->backendList), "changed",
			G_CALLBACK(settingsBackendChanged), ctx);
}

void populateSettings(GUIContext *ctx)
{
	/// The list of backends
	char const **backends = NULL;
	/// The number of bakends
	int count = 0;
	/// The current backend
	int current = -1;

	backends = audioGetBackends(ctx->audio, &count, &current);
	gtk_combo_box_text_remove_all(ctx->backendList);
	for(int i = 0; i < count; i++) {
		gtk_combo_box_text_append(ctx->backendList, NULL, backends[i]);
	}
	if(current != -1) {
		/* This will populate the device list automatically, because the changed
		event will be triggered. */
		gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->backendList), current);
	}

	if(backends) {
		free(backends);
	}
}

void windowClose()
{
    gtk_main_quit();
}

void recordClicked(GtkButton *button, gpointer contextPtr)
{
	GUIContext *ctx = (GUIContext *) contextPtr;

	if(ctx->keepRecording) {
		stopRecording(ctx);
	} else {
		startRecording(ctx);
	}
}

void drawGuitar(GtkWidget *widget, cairo_t *cr, gpointer userData)
{
	guint width, height;
	GtkStyleContext *context;
	RsvgHandle *rsvgHandle;
	RsvgDimensionData dimensionData;
	float ratio;

	context = gtk_widget_get_style_context(widget);
	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	rsvgHandle = rsvg_handle_new_from_file(GUITAR_NECK_FILE, NULL);
	if(!rsvgHandle) {
		fprintf(stderr, "Could not open the neck file.\n");
		return;
	}

	rsvg_handle_get_dimensions(rsvgHandle, &dimensionData);
	ratio = dimensionData.width / (float) dimensionData.height;

	if((ratio * height) < width) {
		ratio = height / (float) dimensionData.height;
	} else {
		ratio = width / (float) dimensionData.width;
	}
	cairo_scale(cr, ratio, ratio);

	rsvg_handle_render_cairo_sub(rsvgHandle, cr, GUITAR_NECK_LAYER);

	char dotId[GUITAR_DOTS_MAXLENGTH];
	for(int i = 0; i < GUITAR_STRINGS; i++) {
		if(gFrets[i] >= 0 && gFrets[i] <= GUITAR_FRETS) {
			// i + 1 because string number goes from 1 to 6 in SVG file
			snprintf(dotId, GUITAR_DOTS_MAXLENGTH, GUITAR_DOTS_TEMPLATE, i + 1,
					gFrets[i]);
			rsvg_handle_render_cairo_sub(rsvgHandle, cr, dotId);
		}
	}

	g_object_unref(rsvgHandle);
}

void menuQuit(GtkMenuItem *menuItem, gpointer mainWindow)
{
	gtk_widget_destroy(GTK_WIDGET(mainWindow));
	gtk_main_quit();
}

void menuShowSettings(GtkMenuItem *menuItem, gpointer settingsWindow)
{
	gtk_window_present(GTK_WINDOW(settingsWindow));
}

void settingsApply(GtkButton *button, gpointer contextPtr)
{
	/// The GUIContext instance
	GUIContext *ctx = (GUIContext *) contextPtr;
	/// The newly chosen backend
	gchar *backend;
	/// The newly chosen input device
	gchar *device;

	char wasRecording = ctx->keepRecording;

	if(wasRecording) {
		stopRecording(ctx);
	}

	backend = gtk_combo_box_text_get_active_text(ctx->backendList);
	device = gtk_combo_box_text_get_active_text(ctx->deviceList);

	if(!audioSetBackend(ctx->audio, backend) &&
			audioSetDevice(ctx->audio, device)) {
		// In case it has been disabled...
		gtk_widget_set_sensitive(ctx->recordButton, TRUE);
	} else {
		gtk_widget_set_sensitive(ctx->recordButton, FALSE);

		GtkWidget *dialog = gtk_message_dialog_new(
				GTK_WINDOW(ctx->settingsWindow), GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "An error occurred while "
				"trying to apply these settings. Please check them again.");
		gtk_dialog_run(GTK_DIALOG(dialog));
		// The dialog is blocking
		gtk_widget_destroy(dialog);

		// Prevent the dialog from closing
		return;
	}

	if(wasRecording) {
		startRecording(ctx);
	}

	gtk_widget_hide(ctx->settingsWindow);
}

void settingsCancel(gpointer contextPtr)
{
	GUIContext *ctx = (GUIContext *) contextPtr;

	/* Repopulate to make sure that next time the settings dialog will be
	opened, it will have the current and valid settings. */
	populateSettings(ctx);

	gtk_widget_hide(ctx->settingsWindow);
}

void settingsBackendChanged(GtkComboBox *backendList, gpointer contextPtr)
{
	/// The GUIContext instance
	GUIContext *ctx = (GUIContext *)contextPtr;
	/// The name of the selected backend
	gchar *backend;
	/// The list of devices
	char **devices;
	/// The number of devices
	int count;
	/// The default device
	int def;

	if(!ctx) {
		return;
	}

	backend = gtk_combo_box_text_get_active_text(ctx->backendList);
	if(!backend) {
		// Should never happen, but checking is always better...
		return;
	}
	devices = audioGetBackendDevices(backend, &count, &def);
	gtk_combo_box_text_remove_all(ctx->deviceList);

	if(!devices) {
		GtkWidget *dialog = gtk_message_dialog_new(
				GTK_WINDOW(ctx->settingsWindow), GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Could not retreive device "
				"list for backend %s. Please make sure any required server "
				"(e. g. Jackd) is running.", backend);
		gtk_dialog_run(GTK_DIALOG(dialog));
		// The dialog is blocking
		gtk_widget_destroy(dialog);

		// Disable saving and device changing
		gtk_widget_set_sensitive(ctx->applyButton, FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(ctx->deviceList), FALSE);

		return;
	}

	// Re-enable saving and device changing in case they had been disabled
	gtk_widget_set_sensitive(ctx->applyButton, TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(ctx->deviceList), TRUE);

	for(int i = 0; i < count; i++) {
		gtk_combo_box_text_append(ctx->deviceList, NULL, devices[i]);
		free(devices[i]);
	}
	if(def != -1) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(ctx->deviceList), def);
	}

	free(devices);
}

gboolean windowPreventDelete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gtk_widget_hide(widget);
	return TRUE;
}

gboolean queueRedraw(gpointer userData)
{
	if(gDrawArea) {
		gtk_widget_queue_draw(gDrawArea);
	}

	return G_SOURCE_REMOVE;
}

void startRecording(GUIContext *ctx)
{
	const char *threadName = "RecordingThread";

	if(ctx->recordingThread && ctx->keepRecording) {
		// Very likely we're already recording
		return;
	}

	ctx->keepRecording = 1;
	g_thread_new(threadName, recordingWorker, ctx);

	gtk_button_set_label(GTK_BUTTON(ctx->recordButton), "gtk-media-stop");
}

void stopRecording(GUIContext *ctx)
{
	ctx->keepRecording = 0;

	if(ctx->recordingThread) {
		g_thread_join(ctx->recordingThread);

		/* From the documentation:
		«Note that g_thread_join() implicitly unrefs the GThread as well.» */
		ctx->recordingThread = 0;

		ctx->recordingThread = 0;
	}

	gtk_button_set_label(GTK_BUTTON(ctx->recordButton), "gtk-media-record");
	guiResetHighlights();
}

gpointer recordingWorker(gpointer contextPtr)
{
	GUIContext *ctx = (GUIContext *) contextPtr;

	if(!audioRecord(ctx->audio, &ctx->keepRecording)) {
		printf("Report error in some way to GUI!\n");
	}

	return NULL;
}
