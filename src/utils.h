#ifndef _UTILS_H
#define _UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

	gboolean file_has_extension(const char *filename, const char *ext);
	gboolean gtk_filename_filter(const char *filename,
				     GtkFileFilter * filter);
	GdkPixbuf *load_pixbuf_from_archive(const char *archname,
					    const char *archpath);
	GdkPixbufAnimation *load_anime_from_archive(const char *archname,
						    const char *archpath);
	GList *get_filelist_from_entry(const char *entry,
				       GtkFileFilter * filter);
	void *load_libunrar(void);
	GtkFileFilter *load_gdkpixbuf_filename_filter(void);

#ifdef __cplusplus
}
#endif
#endif
