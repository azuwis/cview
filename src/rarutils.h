#ifndef _RARUTILS_H
#define _RARUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

	GdkPixbuf *load_pixbuf_from_archive(const char *archname,
					    const char *archpath);
	GdkPixbufAnimation *load_anime_from_archive(const char *archname,
						    const char *archpath);
	GList *get_filelist_from_archive(const char *archname);
	void *load_libunrar(void *handle);

#ifdef __cplusplus
}
#endif
#endif
