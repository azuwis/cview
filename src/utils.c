#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <gtk/gtk.h>
#include <zzip/lib.h>
#include "unrar.h"
#include "utils.h"

static gboolean file_has_extension(const char *filename, const char *ext)
{
	char *fileext = NULL;
	fileext = strrchr(filename, '.');
	if (fileext != NULL) {
		fileext++;
		if (g_ascii_strcasecmp(fileext, ext) == 0)
			return TRUE;
	}
	return FALSE;
}

static gboolean gtk_filename_filter(char *filename, GtkFileFilter * filter)
{
	gboolean ret = FALSE;
	GtkFileFilterInfo info;

	info.contains = GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_DISPLAY_NAME;
	gchar *lower_name = g_ascii_strdown(filename, -1);
	info.filename = info.display_name = lower_name;

	if (gtk_file_filter_filter(filter, &info)) {
		ret = TRUE;
	}
	g_free(lower_name);
	return ret;
}

static int rarcbpixbuf(UINT msg, LONG UserData, LONG P1, LONG P2)
{
	if (msg == UCM_PROCESSDATA) {
		GError *error = NULL;
		GdkPixbufLoader *loader = (GdkPixbufLoader *) UserData;

		gdk_pixbuf_loader_write(loader, (void *)P1, P2, &error);
		if (error != NULL) {
			g_warning("load image in rar callback failed: %s\n",
				  error->message);
			g_error_free(error);
			return -1;
		}
	}
	return 0;
}

static void extract_rar_file_into_loader(GdkPixbufLoader * loader,
					 const char *archname,
					 const char *archpath)
{
	struct RAROpenArchiveData arcdata;
	int code = 0;
	int ret;
	HANDLE hrar;

	arcdata.ArcName = (char *)archname;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	hrar = RAROpenArchive(&arcdata);

	if (hrar == NULL && loader == NULL)
		return;
	RARSetCallback(hrar, rarcbpixbuf, (LONG) loader);
	do {
		struct RARHeaderData header;

		if ((ret = RARReadHeader(hrar, &header)) != 0) {
			if (ret != ERAR_UNKNOWN && ret != ERAR_BAD_DATA)
				break;
			RARCloseArchive(hrar);
			//test_rar_file_password(buf, archname, archpath);
			return;
		}
		if (strcasecmp(header.FileName, archpath) == 0) {
			code = RARProcessFile(hrar, RAR_TEST, NULL, NULL);
			break;
		}
	} while (RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	RARCloseArchive(hrar);

	if (code == 22) {
		//test_rar_file_password(buf, archname, archpath);
		return;
	}
	if (code != 0) {
		g_object_unref(loader);
	}
}

#define ZIPBUFSIZE 102400
static void extract_zip_file_into_loader(GdkPixbufLoader * loader,
					 const char *archname,
					 const char *archpath)
{
	if (loader == NULL)
		return;
	ZZIP_DIR *dir = zzip_dir_open(archname, 0);
	ZZIP_FILE *fp = zzip_file_open(dir, archpath, 0);
	if (fp) {
		guchar buf[ZIPBUFSIZE];
		GError *error = NULL;
		zzip_ssize_t len;
		while ((len = zzip_file_read(fp, buf, ZIPBUFSIZE))) {
			gdk_pixbuf_loader_write(loader, buf, len, &error);
			if (error != NULL) {
				g_warning("load image in zip failed: %s\n",
					  error->message);
				g_error_free(error);
				zzip_dir_close(dir);
				return;
			}
		}
		zzip_file_close(fp);
	}
	zzip_dir_close(dir);
}

GdkPixbuf *load_pixbuf_from_archive(const char *archname, const char *archpath)
{
	GError *error = NULL;
	GdkPixbufLoader *loader = NULL;
	GdkPixbuf *pixbuf = NULL;

	loader = gdk_pixbuf_loader_new();
	if (loader == NULL)
		return NULL;

	if (file_has_extension(archname, "rar")) {
		extract_rar_file_into_loader(loader, archname, archpath);
	} else if (file_has_extension(archname, "zip")) {
		extract_zip_file_into_loader(loader, archname, archpath);
	}
	gdk_pixbuf_loader_close(loader, &error);
	if (error != NULL) {
		g_warning("load image \"%s\" in \"%s\" failed: %s\n", archpath,
			  archname, error->message);
		g_error_free(error);
		g_object_unref(loader);
		return NULL;
	}

	pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));
	g_object_unref(loader);
	return pixbuf;
}

GdkPixbufAnimation *load_anime_from_archive(const char *archname,
					    const char *archpath)
{
	GError *error = NULL;
	GdkPixbufLoader *loader = NULL;
	GdkPixbufAnimation *anim = NULL;

	loader = gdk_pixbuf_loader_new();
	if (loader == NULL)
		return NULL;

	if (file_has_extension(archname, "rar")) {
		extract_rar_file_into_loader(loader, archname, archpath);
	} else if (file_has_extension(archname, "zip")) {
		extract_zip_file_into_loader(loader, archname, archpath);
	}
	gdk_pixbuf_loader_close(loader, &error);
	if (error != NULL) {
		g_warning("load image \"%s\" in \"%s\" failed: %s\n", archpath,
			  archname, error->message);
		g_error_free(error);
		g_object_unref(loader);
		return NULL;
	}

	anim = g_object_ref(gdk_pixbuf_loader_get_animation(loader));
	g_object_unref(loader);
	return anim;
}

/* all filelist->data should be g_free()
 * if filter == NULL, all files will be in the list
 */
static GList *get_filelist_from_rar(const char *archname,
				    GtkFileFilter * filter)
{
	GList *filelist = NULL;
	gchar *filename = NULL;

	struct RAROpenArchiveData arcdata;
	int ret;
	HANDLE hrar;

	arcdata.ArcName = (char *)archname;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	hrar = RAROpenArchive(&arcdata);

	if (hrar == NULL)
		return NULL;
	do {
		struct RARHeaderData header;

		if ((ret = RARReadHeader(hrar, &header)) != 0) {
			if (ret != ERAR_UNKNOWN && ret != ERAR_BAD_DATA)
				break;
			RARCloseArchive(hrar);
			//test_rar_file_password(buf, archname, archpath);
			return NULL;
		}
		filename = g_strdup(header.FileName);
		if ((filter != NULL && gtk_filename_filter(filename, filter))
		    || filter == NULL)
			filelist = g_list_append(filelist, filename);
	} while (RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	RARCloseArchive(hrar);

	return filelist;
}

GList *get_filelist_from_zip(const char *archname, GtkFileFilter * filter)
{
	GList *filelist = NULL;
	gchar *filename = NULL;

	ZZIP_DIR *dir = zzip_dir_open(archname, 0);
	if (dir) {
		ZZIP_DIRENT dirent;
		while (zzip_dir_read(dir, &dirent) != 0) {
			filename = g_strdup(dirent.d_name);
			if ((filter != NULL
			     && gtk_filename_filter(filename, filter))
			    || filter == NULL)
				filelist = g_list_append(filelist, filename);
		}
		zzip_dir_close(dir);
	}
	return filelist;

}

GList *get_filelist_from_archive(const char *archname, GtkFileFilter * filter)
{
	if (file_has_extension(archname, "rar")) {
		return get_filelist_from_rar(archname, filter);
	} else if (file_has_extension(archname, "zip")) {
		return get_filelist_from_zip(archname, filter);
	}
	return NULL;
}

/* call dlclose(handle) at the end */
void *load_libunrar(void *handle)
{
	const char *error;
	handle = dlopen("./libunrar.so", RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "%s\n", dlerror());
		exit(EXIT_FAILURE);
	}
	dlerror();		/* Clear any existing error */

	RAROpenArchive = dlsym(handle, "RAROpenArchive");
	RAROpenArchiveEx = dlsym(handle, "RAROpenArchiveEx");
	RARCloseArchive = dlsym(handle, "RARCloseArchive");
	RARReadHeader = dlsym(handle, "RARReadHeader");
	RARReadHeaderEx = dlsym(handle, "RARReadHeaderEx");
	RARProcessFile = dlsym(handle, "RARProcessFile");
	RARProcessFileW = dlsym(handle, "RARProcessFileW");
	RARSetCallback = dlsym(handle, "RARSetCallback");
	RARSetChangeVolProc = dlsym(handle, "RARSetChangeVolProc");
	RARSetProcessDataProc = dlsym(handle, "RARSetProcessDataProc");
	RARSetPassword = dlsym(handle, "RARSetPassword");
	RARGetDllVersion = dlsym(handle, "RARGetDllVersion");

	if ((error = dlerror()) != NULL) {
		fprintf(stderr, "%s\n", error);
		exit(EXIT_FAILURE);
	}
	return handle;
}

GtkFileFilter *load_gdkpixbuf_filename_filter(void)
{
	GtkFileFilter *filter = NULL;
	GSList *formatlist = NULL;
	GSList *formatlisthead = NULL;

	filter = gtk_file_filter_new();
	//gtk_file_filter_add_pixbuf_formats(filter);
	for (formatlist = formatlisthead = gdk_pixbuf_get_formats();
	     formatlist != NULL; formatlist = g_slist_next(formatlist)) {
		GdkPixbufFormat *pixformat =
		    (GdkPixbufFormat *) formatlist->data;
		gchar **extensions =
		    gdk_pixbuf_format_get_extensions(pixformat);
		int i;
		for (i = 0; extensions[i] != NULL; i++) {
			gchar *pattern = g_strconcat("*.", extensions[i], NULL);
			gtk_file_filter_add_pattern(filter, pattern);
			g_free(pattern);
		}
		g_strfreev(extensions);
	}
	g_slist_free(formatlisthead);
	return filter;
}