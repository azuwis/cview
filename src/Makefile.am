bin_PROGRAMS = cview \
			   test

#cview_CPPFLAGS = $(DEPS_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
#cview_CFLAGS = -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
cview_CFLAGS = $(DEPS_CFLAGS)
cview_LDADD = $(DEPS_LIBS)
cview_SOURCES = \
	buffer.c \
	cview.c

test_CFLAGS = $(DEPS_CFLAGS)
test_LDADD = $(DEPS_LIBS)
test_SOURCES = \
	test.c

MAINTAINERCLEANFILES = Makefile.in config.h.in
