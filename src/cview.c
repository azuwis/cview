/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*- */

#include <glib.h>
#include <gtk/gtk.h>
#include <assert.h>
#include <stdio.h>
#include <dlfcn.h>
#include "unrar.h"
#include "config.h"
#include "utils.h"

// Library under test.
#include <gtkimageview/gtkanimview.h>
#include <gtkimageview/gtkimagescrollwin.h>
#include <gtkimageview/gtkimagetooldragger.h>
#include <gtkimageview/gtkimagetoolpainter.h>
#include <gtkimageview/gtkimagetoolselector.h>

// Defines for backwards compatibility with GTK+ 2.6
#if !GTK_CHECK_VERSION(2,8,0)
#define GTK_STOCK_FULLSCREEN ""
#endif

//////////////////////////////////////////////////////////////////////
///// Global data ////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
static GtkFileChooserDialog *open_dialog = NULL;
static GtkAnimView *view = NULL;
static GtkWindow *main_window;
static GtkActionGroup *default_group = NULL;
static GtkActionGroup *image_group = NULL;
static GtkActionGroup *transform_group = NULL;
static GtkActionGroup *go_group = NULL;
static gboolean is_fullscreen = FALSE;
static GtkWidget *statusbar = NULL;
static GList *entry_list = NULL;
static GList *current_entry = NULL;
static GList *image_list = NULL;
static GList *current_image = NULL;
static gboolean rar_support = FALSE;
static GtkFileFilter *gdkpixbuf_filter = NULL;
/* total and current number of images of current entry*/
static gint n_images = 0;
static gint i_images = 0;

// Label that displays the active selection.
static GtkWidget *sel_info_label = NULL;

// Tools
static GtkIImageTool *dragger = NULL;
static GtkIImageTool *selector = NULL;
static GtkIImageTool *painter = NULL;

// Context ID:s for the Statusbar
int help_msg_cid = -1;
int image_info_cid = -1;

//////////////////////////////////////////////////////////////////////
///// Opener dialog //////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
static void init_open_dialog()
{
	open_dialog = (GtkFileChooserDialog *)
	    gtk_file_chooser_dialog_new("Open Image", main_window,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(open_dialog),
					     TRUE);
}

//////////////////////////////////////////////////////////////////////
///// ImageViewerApp /////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
static void push_image_info(char *basename, GdkPixbufAnimation * anim)
{
	int width = gdk_pixbuf_animation_get_width(anim);
	int height = gdk_pixbuf_animation_get_height(anim);
	char *msg = g_strdup_printf("[%d/%d] %s, %d x %d pixels",
				    i_images, n_images, basename, width,
				    height);
	gtk_statusbar_push(GTK_STATUSBAR(statusbar), image_info_cid, msg);
	g_free(msg);
}

static void load_image(const char *entry, const char *path)
{
	GdkPixbufAnimation *anim = NULL;

	if (path == NULL) {
		anim = gdk_pixbuf_animation_new_from_file(entry, NULL);
	} else if (g_str_has_suffix(entry, "/")) {
		gchar *full_name = g_strconcat(entry, path, NULL);
		anim = gdk_pixbuf_animation_new_from_file(full_name, NULL);
		g_free(full_name);
	} else {
		anim = load_anime_from_archive(entry, path);
	}
	if (!anim) {
		fprintf(stderr, "Could not load image.\n");
		return;
	}
	gtk_anim_view_set_anim(view, anim);
	g_object_unref(anim);

	char *image_basename = NULL;
	gchar *full_path = NULL;
	char *arch_basename = g_path_get_basename(entry);
	if (path == NULL) {
		full_path = arch_basename;
	} else {
		image_basename = g_path_get_basename(path);
		full_path =
		    g_strdup_printf("%s/%s", arch_basename, image_basename);
		g_free(image_basename);
		g_free(arch_basename);
	}
	gchar *title = g_strdup_printf("%s: %s", PACKAGE_NAME, full_path);
	gtk_window_set_title(main_window, title);
	push_image_info(full_path, anim);
	g_free(title);
	g_free(full_path);

	gtk_action_group_set_sensitive(image_group, TRUE);

	/* Only active the transform_group if the loaded object is a single
	   image -- transformations cannot be applied to animations. */
	gboolean is_image = gdk_pixbuf_animation_is_static_image(anim);
	gtk_action_group_set_sensitive(transform_group, is_image);
}

/* TODO add regular file and directory support */
static void load_entry(const char *filename)
{
	g_list_foreach(image_list, (GFunc) g_free, NULL);
	g_list_free(image_list);
	current_image = image_list = NULL;

	if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
		if (rar_support && file_has_extension(filename, "rar")) {
			image_list =
			    get_filelist_from_rar(filename, gdkpixbuf_filter);
		} else if (file_has_extension(filename, "zip")) {
			image_list =
			    get_filelist_from_zip(filename, gdkpixbuf_filter);
		} else if (gtk_filename_filter(filename, gdkpixbuf_filter)) {
			n_images = i_images = 1;
			load_image(filename, NULL);
			return;
		}
	} else if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
		image_list = get_filelist_from_dir(filename, gdkpixbuf_filter);
	}

	n_images = g_list_length(image_list);

	current_image = g_list_first(image_list);
	if (current_image != NULL) {
		i_images = 1;
		load_image(filename, current_image->data);
	}
}

//////////////////////////////////////////////////////////////////////
///// Callbacks //////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
static void sel_changed_cb(GtkImageToolSelector * selector, GtkLabel * label)
{
	GdkRectangle sel;
	gtk_image_tool_selector_get_selection(selector, &sel);
	if (!sel.width || !sel.height) {
		gtk_label_set_text(label, "");
	} else {
		char *text = g_strdup_printf("%s", gdk_rectangle_to_str(sel));
		gtk_label_set_text(label, text);
		g_free(text);
	}
}

static void change_image_tool_cb(GtkAction * action, GtkRadioAction * current)
{
	int value = gtk_radio_action_get_current_value(current);
	GtkIImageTool *tool = selector;
	if (value == 10)
		tool = dragger;
	else if (value == 30)
		tool = painter;
	gtk_image_view_set_tool(GTK_IMAGE_VIEW(view), tool);
	if (value == 20)
		sel_changed_cb(GTK_IMAGE_TOOL_SELECTOR(selector),
			       GTK_LABEL(sel_info_label));
	else
		gtk_label_set_text(GTK_LABEL(sel_info_label), "");
}

static void zoom_in_cb()
{
	gtk_image_view_zoom_in(GTK_IMAGE_VIEW(view));
}

static void zoom_out_cb()
{
	gtk_image_view_zoom_out(GTK_IMAGE_VIEW(view));
}

static void zoom_100_cb()
{
	gtk_image_view_set_zoom(GTK_IMAGE_VIEW(view), 1.0);
}

static void zoom_to_fit_cb()
{
	gtk_image_view_set_fitting(GTK_IMAGE_VIEW(view), TRUE);
}

static void open_image_cb(GtkAction * action)
{
	GSList *filenames = NULL;
	GSList *filenames_iter = NULL;
	g_list_foreach(entry_list, (GFunc) g_free, NULL);
	g_list_free(entry_list);
	current_entry = entry_list = NULL;
	g_list_foreach(image_list, (GFunc) g_free, NULL);
	g_list_free(image_list);
	current_image = image_list = NULL;
	if (!open_dialog)
		init_open_dialog();
	if (gtk_dialog_run(GTK_DIALOG(open_dialog)) == GTK_RESPONSE_ACCEPT) {
		filenames =
		    gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER
						   (open_dialog));
		for (filenames_iter = filenames; filenames_iter != NULL;
		     filenames_iter = g_slist_next(filenames_iter)) {
			entry_list =
			    g_list_append(entry_list, filenames_iter->data);
		}
		g_slist_free(filenames);
		current_entry = g_list_first(entry_list);
		load_entry(current_entry->data);
	}
	gtk_widget_hide(GTK_WIDGET(open_dialog));
}

static void fullscreen_cb()
{
	// I do not have the patience to implement all things you do to
	// fullscreen for real. This is a faked approximation.
	is_fullscreen = !is_fullscreen;
	if (is_fullscreen)
		gtk_window_fullscreen(main_window);
	else
		gtk_window_unfullscreen(main_window);

	gtk_image_view_set_show_cursor(GTK_IMAGE_VIEW(view), !is_fullscreen);
	gtk_image_view_set_show_frame(GTK_IMAGE_VIEW(view), !is_fullscreen);
	gtk_image_view_set_black_bg(GTK_IMAGE_VIEW(view), is_fullscreen);
}

static void transform_cb()
{
	GdkPixbuf *pixbuf = gtk_image_view_get_pixbuf(GTK_IMAGE_VIEW(view));
	guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
	int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
	int x, y, n;
	for (y = 0; y < gdk_pixbuf_get_height(pixbuf); y++)
		for (x = 0; x < gdk_pixbuf_get_width(pixbuf); x++) {
			guchar *p = pixels + y * rowstride + x * n_channels;
			for (n = 0; n < 3; n++)
				p[n] ^= 0xff;
		}
	gtk_image_view_damage_pixels(GTK_IMAGE_VIEW(view), NULL);
}

static void change_zoom_quality_cb(GtkAction * action, GtkRadioAction * current)
{
	if (gtk_radio_action_get_current_value(current))
		gtk_image_view_set_interpolation(GTK_IMAGE_VIEW(view),
						 GDK_INTERP_BILINEAR);
	else
		gtk_image_view_set_interpolation(GTK_IMAGE_VIEW(view),
						 GDK_INTERP_NEAREST);
}

static void change_transp_type_cb(GtkAction * action, GtkRadioAction * current)
{
	int color = 0;
	GtkImageTransp transp = gtk_radio_action_get_current_value(current);
	if (transp == GTK_IMAGE_TRANSP_COLOR)
		color = 0x000000;
	gtk_image_view_set_transp(GTK_IMAGE_VIEW(view), transp, color);
}

static void go_next_cb(GtkAction * action)
{
	GList *next_image = NULL;
	next_image = g_list_next(current_image);
	if (next_image == NULL) {
		if (g_list_next(current_entry) != NULL) {
			current_entry = g_list_next(current_entry);
			load_entry(current_entry->data);
		} else {
			fprintf(stderr, "Reach end of entry_list.\n");
		}
		return;
	} else {
		current_image = next_image;
		i_images++;
		load_image(current_entry->data, current_image->data);
	}
}

static void go_prev_cb(GtkAction * action)
{
	GList *prev_image = NULL;
	prev_image = g_list_previous(current_image);
	if (prev_image == NULL) {
		if (g_list_previous(current_entry) != NULL) {
			current_entry = g_list_previous(current_entry);
			load_entry(current_entry->data);
		} else {
			fprintf(stderr, "Reach beginning of entry_list.\n");
		}
		return;
	} else {
		current_image = prev_image;
		i_images--;
		load_image(current_entry->data, current_image->data);
	}
}

static void menu_item_select_cb(GtkMenuItem * proxy)
{
	GtkAction *action = g_object_get_data(G_OBJECT(proxy), "gtk-action");

	char *msg;
	g_object_get(G_OBJECT(action), "tooltip", &msg, NULL);

	if (msg) {
		gtk_statusbar_push(GTK_STATUSBAR(statusbar), help_msg_cid, msg);
		g_free(msg);
	}
}

static void menu_item_deselect_cb(GtkMenuItem * item)
{
	gtk_statusbar_pop(GTK_STATUSBAR(statusbar), help_msg_cid);
}

static void
connect_proxy_cb(GtkUIManager * ui, GtkAction * action, GtkWidget * proxy)
{
	if (!GTK_IS_MENU_ITEM(proxy))
		return;
	g_signal_connect(proxy, "select", G_CALLBACK(menu_item_select_cb),
			 NULL);
	g_signal_connect(proxy, "deselect", G_CALLBACK(menu_item_deselect_cb),
			 NULL);
}

static void
disconnect_proxy_cb(GtkUIManager * ui, GtkAction * action, GtkWidget * proxy)
{
	if (!GTK_IS_MENU_ITEM(proxy))
		return;
	g_signal_handlers_disconnect_by_func(proxy,
					     G_CALLBACK(menu_item_select_cb),
					     NULL);
	g_signal_handlers_disconnect_by_func(proxy,
					     G_CALLBACK(menu_item_deselect_cb),
					     NULL);
}

static void zoom_changed_cb(GtkImageView * view, GtkLabel * label)
{
	gdouble zoom = gtk_image_view_get_zoom(view);
	char *text = g_strdup_printf("%d%%", (int)(zoom * 100.0));
	gtk_label_set_text(label, text);
	g_free(text);
}

static void kill_app_cb(void)
{
	/* Kill the widgets. */
	gtk_widget_destroy(GTK_WIDGET(main_window));
	if (open_dialog)
		gtk_widget_destroy(GTK_WIDGET(open_dialog));
	gtk_main_quit();
}

//////////////////////////////////////////////////////////////////////
///// MainWindow /////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
static GtkWindow *main_window_new(GtkWidget * widget, int width, int height)
{
	GtkWindow *window = (GtkWindow *) gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(window, width, height);
	gtk_container_add(GTK_CONTAINER(window), widget);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(kill_app_cb), NULL);
	return window;
}

//////////////////////////////////////////////////////////////////////
///// UI Setup ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
static const GtkActionEntry default_actions[] = {
	{"FileMenu", NULL, "_File"},
	{
	 "Open",
	 GTK_STOCK_OPEN,
	 "_Open",
	 NULL,
	 "Open an image",
	 G_CALLBACK(open_image_cb)
	 },
	{
	 "Quit",
	 GTK_STOCK_QUIT,
	 "_Quit",
	 NULL,
	 "Quit the program",
	 G_CALLBACK(kill_app_cb)
	 },
	{"EditMenu", NULL, "_Edit"},
	{"GoMenu", NULL, "_Go"},
	{"ViewMenu", NULL, "_View"},
	{"TranspMenu", NULL, "_Transparency"}
};

static const GtkRadioActionEntry quality_actions[] = {
	{
	 "QualityHigh",
	 NULL,
	 "_High Quality",
	 NULL,
	 "Use high quality zoom",
	 TRUE},
	{
	 "QualityLow",
	 NULL,
	 "_Low Quality",
	 NULL,
	 "Use low quality zoom",
	 FALSE}
};

static const GtkRadioActionEntry transp_actions[] = {
	{
	 "TranspGrid",
	 NULL,
	 "Square _Grid",
	 NULL,
	 "Draw a grid on transparent parts",
	 GTK_IMAGE_TRANSP_GRID},
	{
	 "TranspBackground",
	 NULL,
	 "_Background",
	 NULL,
	 "Draw background color on transparent parts",
	 GTK_IMAGE_TRANSP_BACKGROUND},
	{
	 "TranspBlack",
	 NULL,
	 "_Black",
	 NULL,
	 "Draw black color on transparent parts",
	 GTK_IMAGE_TRANSP_COLOR}
};

static const GtkActionEntry image_actions[] = {
	{
	 "ZoomIn",
	 GTK_STOCK_ZOOM_IN,
	 "Zoom _In",
	 "<control>plus",
	 "Zoom in one step",
	 G_CALLBACK(zoom_in_cb)
	 },
	{
	 "ZoomOut",
	 GTK_STOCK_ZOOM_OUT,
	 "Zoom _Out",
	 "<control>minus",
	 "Zoom out one step",
	 G_CALLBACK(zoom_out_cb)
	 },
	{
	 "ZoomNormal",
	 GTK_STOCK_ZOOM_100,
	 "_Normal Size",
	 "<control>0",
	 "Set zoom to natural size of the image",
	 G_CALLBACK(zoom_100_cb)
	 },
	{
	 "ZoomFit",
	 GTK_STOCK_ZOOM_FIT,
	 "Best _Fit",
	 NULL,
	 "Adapt zoom to fit image",
	 G_CALLBACK(zoom_to_fit_cb)
	 },
	{
	 "Fullscreen",
	 GTK_STOCK_FULLSCREEN,
	 "_Fullscreen Mode",
	 "F11",
	 "View image in fullscreen",
	 G_CALLBACK(fullscreen_cb)
	 }
};

static const GtkRadioActionEntry image_tools[] = {
	{
	 "DraggerTool",
	 GTK_STOCK_REFRESH,
	 "_Drag",
	 NULL,
	 "Use the hand tool",
	 10},
	{
	 "SelectorTool",
	 GTK_STOCK_MEDIA_PAUSE,
	 "_Select",
	 NULL,
	 "Use the rectangular selection tool",
	 20},
	{
	 "PainterTool",
	 GTK_STOCK_MEDIA_PLAY,
	 "_Paint",
	 NULL,
	 "Use the painter tool",
	 30}
};

static const GtkActionEntry transform_actions[] = {
	{
	 "Transform",
	 NULL,
	 "_Transform",
	 "<control>T",
	 "Apply an XOR transformation to the image",
	 G_CALLBACK(transform_cb)
	 }
};

static const GtkActionEntry go_actions[] = {
	{
	 "GoNext", NULL,
	 "_Next Page",
	 "N",
	 "Go to next page",
	 G_CALLBACK(go_next_cb)
	 },
	{
	 "GoPrev",
	 NULL,
	 "_Prev Page",
	 "P",
	 "Go to prev page",
	 G_CALLBACK(go_prev_cb)
	 }
};

gchar *ui_info =
    "<ui>"
    "  <menubar name = 'MenuBar'>"
    "    <menu action = 'FileMenu'>"
    "      <menuitem action = 'Open'/>"
    "      <menuitem action = 'Quit'/>"
    "    </menu>"
    "    <menu action = 'EditMenu'>"
    "      <menuitem action = 'Transform'/>"
    "      <separator/>"
    "      <menuitem action = 'DraggerTool'/>"
    "      <menuitem action = 'SelectorTool'/>"
    "      <menuitem action = 'PainterTool'/>"
    "    </menu>"
    "    <menu action = 'GoMenu'>"
    "      <menuitem action = 'GoNext'/>"
    "      <menuitem action = 'GoPrev'/>"
    "    </menu>"
    "    <menu action = 'ViewMenu'>"
    "      <menuitem action = 'Fullscreen'/>"
    "      <separator/>"
    "      <menuitem action = 'ZoomIn'/>"
    "      <menuitem action = 'ZoomOut'/>"
    "      <menuitem action = 'ZoomNormal'/>"
    "      <menuitem action = 'ZoomFit'/>"
    "      <separator/>"
    "      <menu action = 'TranspMenu'>"
    "        <menuitem action = 'TranspGrid'/>"
    "        <menuitem action = 'TranspBackground'/>"
    "        <menuitem action = 'TranspBlack'/>"
    "      </menu>"
    "      <separator/>"
    "      <menuitem action = 'QualityHigh'/>"
    "      <menuitem action = 'QualityLow'/>"
    "    </menu>"
    "  </menubar>"
    "  <toolbar name = 'ToolBar'>"
    "    <toolitem action='Quit'/>"
    "    <toolitem action='Open'/>"
    "    <separator/>"
    "    <toolitem action='DraggerTool'/>"
    "    <toolitem action='SelectorTool'/>"
    "    <toolitem action='PainterTool'/>"
    "    <separator/>"
    "    <toolitem action='ZoomIn'/>"
    "    <toolitem action='ZoomOut'/>"
    "    <toolitem action='ZoomNormal'/>"
    "    <toolitem action='ZoomFit'/>" "  </toolbar>" "</ui>";

static void parse_ui(GtkUIManager * uimanager)
{
	GError *err;
	if (!gtk_ui_manager_add_ui_from_string(uimanager, ui_info, -1, &err)) {
		g_warning("Unable to create menus: %s", err->message);
		g_free(err);
	}
}

static void add_action_groups(GtkUIManager * uimanager)
{
	// Setup the default group.
	default_group = gtk_action_group_new("default");
	gtk_action_group_add_actions(default_group,
				     default_actions,
				     G_N_ELEMENTS(default_actions), NULL);
	gtk_action_group_add_radio_actions(default_group,
					   image_tools,
					   G_N_ELEMENTS(image_tools),
					   10,
					   G_CALLBACK(change_image_tool_cb),
					   NULL);
	gtk_ui_manager_insert_action_group(uimanager, default_group, 0);

	// Setup the image group.
	image_group = gtk_action_group_new("image");
	gtk_action_group_add_actions(image_group,
				     image_actions,
				     G_N_ELEMENTS(image_actions), NULL);
	gtk_action_group_add_radio_actions(image_group,
					   quality_actions,
					   G_N_ELEMENTS(quality_actions),
					   TRUE,
					   G_CALLBACK(change_zoom_quality_cb),
					   NULL);
	gtk_action_group_add_radio_actions(image_group,
					   transp_actions,
					   G_N_ELEMENTS(transp_actions),
					   GTK_IMAGE_TRANSP_GRID,
					   G_CALLBACK(change_transp_type_cb),
					   NULL);
	gtk_action_group_set_sensitive(image_group, FALSE);
	gtk_ui_manager_insert_action_group(uimanager, image_group, 0);

	// Transform group
	transform_group = gtk_action_group_new("transform");
	gtk_action_group_add_actions(transform_group,
				     transform_actions,
				     G_N_ELEMENTS(transform_actions), NULL);
	gtk_action_group_set_sensitive(transform_group, FALSE);
	gtk_ui_manager_insert_action_group(uimanager, transform_group, 0);

	// Go group
	go_group = gtk_action_group_new("go");
	gtk_action_group_add_actions(go_group,
				     go_actions,
				     G_N_ELEMENTS(go_actions), NULL);
	//gtk_action_group_set_sensitive (go_group, FALSE);
	gtk_ui_manager_insert_action_group(uimanager, go_group, 0);
}

static GtkWidget *setup_layout(GtkUIManager * uimanager)
{
	GtkWidget *box = gtk_vbox_new(FALSE, 0);

	GtkWidget *menu = gtk_ui_manager_get_widget(uimanager, "/MenuBar");
	gtk_box_pack_start(GTK_BOX(box), menu, FALSE, FALSE, 0);

	GtkWidget *toolbar = gtk_ui_manager_get_widget(uimanager, "/ToolBar");
	gtk_box_pack_start(GTK_BOX(box), toolbar, FALSE, FALSE, 0);

	GtkWidget *scroll_win = gtk_image_scroll_win_new(GTK_IMAGE_VIEW(view));

	gtk_box_pack_start(GTK_BOX(box), scroll_win, TRUE, TRUE, 0);

	statusbar = gtk_statusbar_new();

	// A label in the statusbar that displays the current selection if
	// there is one.
	GtkWidget *sel_info_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(sel_info_frame), GTK_SHADOW_IN);

	sel_info_label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(sel_info_frame), sel_info_label);

	g_signal_connect(G_OBJECT(selector), "selection_changed",
			 G_CALLBACK(sel_changed_cb), sel_info_label);

	gtk_box_pack_start(GTK_BOX(statusbar), sel_info_frame, FALSE, FALSE, 0);

	// A label in the statusbar that displays the current zoom. It
	// updates its text when the zoom-changed signal is fired from the
	// view.
	GtkWidget *zoom_info_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(zoom_info_frame), GTK_SHADOW_IN);

	GtkWidget *zoom_info_label = gtk_label_new("100%");
	gtk_container_add(GTK_CONTAINER(zoom_info_frame), zoom_info_label);

	g_signal_connect(G_OBJECT(view), "zoom_changed",
			 G_CALLBACK(zoom_changed_cb), zoom_info_label);

	gtk_box_pack_start(GTK_BOX(statusbar), zoom_info_frame, FALSE, FALSE,
			   0);

	gtk_box_pack_end(GTK_BOX(box), statusbar, FALSE, FALSE, 0);
	return box;
}

static void setup_main_window()
{
	GtkUIManager *uimanager = gtk_ui_manager_new();

	g_signal_connect(uimanager, "connect_proxy",
			 G_CALLBACK(connect_proxy_cb), NULL);
	g_signal_connect(uimanager, "disconnect_proxy",
			 G_CALLBACK(disconnect_proxy_cb), NULL);

	add_action_groups(uimanager);
	parse_ui(uimanager);

	GtkAccelGroup *accels = gtk_ui_manager_get_accel_group(uimanager);
	assert(accels);

	GtkWidget *vbox = setup_layout(uimanager);
	main_window = main_window_new(vbox, 700, 500);
	gtk_window_add_accel_group(main_window, accels);

	gtk_widget_grab_focus(GTK_WIDGET(view));

	// Setup context ID:s
	help_msg_cid = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
						    "help_msg");
	image_info_cid = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar),
						      "image_info");
	g_object_unref(uimanager);
}

int main(int argc, char *argv[])
{
	void *handle = load_libunrar();
	if (handle != NULL)
		rar_support = TRUE;
	else
		fprintf(stderr,
			"RAR support is disabled, libunrar.so not found.\n");

	gchar **filenames = NULL;
	gchar **filenames_iter = NULL;
	GOptionEntry options[] = {
		{
		 G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY,
		 &filenames, NULL, "[FILE...]"}
		,
		{NULL}
	};
	GOptionContext *ctx = g_option_context_new("Sample image viewer");
	g_option_context_add_main_entries(ctx, options, "example1");
	g_option_context_parse(ctx, &argc, &argv, NULL);
	g_option_context_free(ctx);

	gtk_init(&argc, &argv);

	view = GTK_ANIM_VIEW(gtk_anim_view_new());

	dragger = gtk_image_tool_dragger_new(GTK_IMAGE_VIEW(view));
	selector = gtk_image_tool_selector_new(GTK_IMAGE_VIEW(view));
	painter = gtk_image_tool_painter_new(GTK_IMAGE_VIEW(view));

	setup_main_window();

	gdkpixbuf_filter = load_gdkpixbuf_filename_filter();
	if (filenames) {
		for (filenames_iter = filenames; *filenames_iter != NULL;
		     filenames_iter++) {
			entry_list = g_list_append(entry_list, *filenames_iter);
		}
		current_entry = entry_list;
		load_entry(current_entry->data);
	}

	gtk_widget_show_all(GTK_WIDGET(main_window));
	gtk_main();
	if (handle != NULL)
		dlclose(handle);
	return 0;
}
