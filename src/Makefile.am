bin_PROGRAMS = cview

#cview_CPPFLAGS = $(DEPS_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
#cview_CFLAGS = $(DEPS_CFLAGS) -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
cview_CFLAGS = $(DEPS_CFLAGS) -Wextra -Wno-missing-field-initializers
cview_LDADD = $(DEPS_LIBS)
cview_SOURCES = \
		rarutils.c \
		cview.c

MAINTAINERCLEANFILES = Makefile.in config.h.in
