#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <gtk/gtk.h>
#include "unrar.h"
#include "rarutils.h"

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

static void extract_rar_file_into_pixbuf(GdkPixbufLoader * loader,
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

GdkPixbufAnimation *load_anime_from_archive(const char *archname,
					    const char *archpath)
{
	GError *error = NULL;
	GdkPixbufLoader *loader;
	GdkPixbufAnimation *pixbuf;

	loader = gdk_pixbuf_loader_new();
	extract_rar_file_into_pixbuf(loader, archname, archpath);
	if (!gdk_pixbuf_loader_close(loader, &error)) {
		g_warning("load image in archive failed: %s\n", error->message);
		g_free(error);
		g_object_unref(loader);
		return NULL;
	}

	pixbuf = g_object_ref(gdk_pixbuf_loader_get_animation(loader));
	g_free(error);
	g_object_unref(loader);
	return pixbuf;
}

GList *get_filelist_from_archive(const char *archname)
{
	GList *filelist;

	struct RAROpenArchiveData arcdata;
	int ret;
	HANDLE hrar;

	arcdata.ArcName = (char *)archname;
	arcdata.OpenMode = RAR_OM_EXTRACT;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;
	hrar = RAROpenArchive(&arcdata);

	if (hrar == NULL)
		return;
	do {
		struct RARHeaderData header;

		if ((ret = RARReadHeader(hrar, &header)) != 0) {
			if (ret != ERAR_UNKNOWN && ret != ERAR_BAD_DATA)
				break;
			RARCloseArchive(hrar);
			//test_rar_file_password(buf, archname, archpath);
			return;
		}
		g_list_append(filelist, header.FileName);
	} while (RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);
	RARCloseArchive(hrar);

	return filelist;
}

void *load_libunrar(void *handle)
{
	/* call dlclose() at the end */

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
}
