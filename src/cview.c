/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; coding: utf-8 -*-
 *
 * This program demonstrates the minimum necessary code needed to use
 * GtkImageView.
 **/
#include <stdio.h>
#include <stdlib.h>
#include <gtkimageview/gtkimageview.h>
#include <dlfcn.h>
#include <string.h>
#include "unrar.h"
#include "buffer.h"

int test(char *rarfile)
{
	struct RAROpenArchiveDataEx OpenArchiveData;
	struct RARHeaderDataEx HeaderData;
	HANDLE hArcData;

	memset(&OpenArchiveData, 0, sizeof(OpenArchiveData));

	OpenArchiveData.ArcName = rarfile;	/* name arch */
	OpenArchiveData.CmtBuf = NULL;
	OpenArchiveData.OpenMode = RAR_OM_LIST;

	hArcData = RAROpenArchiveEx(&OpenArchiveData);
	if (OpenArchiveData.OpenResult != 0) {
		fprintf(stderr, "error archive open (%d)\n",
			OpenArchiveData.OpenResult);
		return 1;
	}
	printf("%s\n", OpenArchiveData.ArcName);

	RARCloseArchive(hArcData);
	printf("%d\n", RARGetDllVersion());
}

static int rarcbproc(UINT msg, LONG UserData, LONG P1, LONG P2)
{
	if (msg == UCM_PROCESSDATA) {
		buffer *buf = (buffer *) UserData;

		if (buffer_append_memory(buf, (void *)P1, P2) == -1) {
			return -1;
		}
	}
	return 0;
}

static int rarcbpixbuf(UINT msg, LONG UserData, LONG P1, LONG P2)
{
	GError *error = NULL;
	if (msg == UCM_PROCESSDATA) {
		GdkPixbufLoader *loader = (GdkPixbufLoader *) UserData;

		if (!gdk_pixbuf_loader_write(loader, (void *)P1, P2, &error)) {
			g_warning("loading image in rar callback failed\n");
			g_free(error);
			return -1;
		}
	}
	g_free(error);
	return 0;
}

void extract_rar_file_into_buffer(buffer * buf,
				  const char *archname, const char *archpath)
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

	if (hrar == NULL)
		return;
	RARSetCallback(hrar, rarcbproc, (LONG) buf);
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

			buffer_prepare_copy(buf, header.UnpSize + 1);

			if (buf->ptr == NULL) {
				RARCloseArchive(hrar);
				return;
			}

			buf->ptr[header.UnpSize] = '\0';
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
		free(buf->ptr);
		buf->ptr = NULL;
		buf->size = buf->used = 0;
	}
}

void extract_rar_file_into_pixbuf(GdkPixbufLoader * loader,
				  const char *archname, const char *archpath)
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

	if (hrar == NULL)
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

			//if (buf->ptr == NULL) {
			//    RARCloseArchive(hrar);
			//    return;
			//}

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

GdkPixbuf *load_pixbuf_from_archive(const char *archname, const char *archpath)
{
	GError *error = NULL;
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;

	loader = gdk_pixbuf_loader_new();
	extract_rar_file_into_pixbuf(loader, archname, archpath);
	gdk_pixbuf_loader_close(loader, &error);

	pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));
	g_object_unref(loader);
	return pixbuf;
}

int main(int argc, char *argv[])
{
	char *fname = argv[1];
	char *archname = argv[2];
	char *archpath = argv[3];

	/* load libunrar START */
	void *handle;		/* call dlclose() at the end */

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
	/* load libunrar END */

	//test(rarfile);

	gtk_init(&argc, &argv);
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *view = gtk_image_view_new();

	GdkPixbuf *pixbuf = load_pixbuf_from_archive(archname, archpath);

	//GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (fname, NULL);
	gtk_image_view_set_pixbuf(GTK_IMAGE_VIEW(view), pixbuf, TRUE);
	gtk_container_add(GTK_CONTAINER(window), view);
	gtk_widget_show_all(window);
	gtk_main();

	dlclose(handle);
	exit(EXIT_SUCCESS);
}
