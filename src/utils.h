#ifndef _UTILS_H
#define _UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

	GdkPixbuf *load_pixbuf_from_archive(const char *archname,
					    const char *archpath);
	GdkPixbufAnimation *load_anime_from_archive(const char *archname,
						    const char *archpath);
	GList *get_filelist_from_archive(const char *archname,
					 GtkFileFilter * filter);
	void *load_libunrar(void *handle);
	GtkFileFilter *load_gdkpixbuf_filename_filter(void);

#ifdef __cplusplus
}
#endif
#endif
