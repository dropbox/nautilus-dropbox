INCLUDES =						\
	-I$(top_srcdir)					\
	-I$(top_builddir)				

nautilus_extensiondir=$(NAUTILUS_EXTENSION_DIR)

nautilus_extension_LTLIBRARIES=libnautilus-dropbox.la

libnautilus_dropbox_la_CFLAGS = 	                \
	-DDATADIR=\"$(datadir)\"			\
	-Wall                                           \
	$(WARN_CFLAGS)                                  \
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(NAUTILUS_CFLAGS)                              \
	$(GLIB_CFLAGS)                                  \
	$(LIBNOTIFY_CFLAGS)

if !DEBUG
libnautilus_dropbox_la_CFLAGS += -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS
endif

libnautilus_dropbox_la_SOURCES = \
	nautilus-dropbox.c       \
	nautilus-dropbox.h       \
	nautilus-dropbox-tray.c  \
	nautilus-dropbox-tray.h  \
	nautilus-dropbox-common.h \
	nautilus-dropbox-common.c \
	nautilus-dropbox-hooks.h \
	nautilus-dropbox-hooks.c \
	nautilus-dropbox-command.h \
	nautilus-dropbox-command.c \
	busy2.h busy.h idle.h logo.h \
	g-util.h g-util.c \
	async-http-downloader.h \
	async-http-downloader.c \
	async-io-coroutine.h \
	dropbox-client-util.c \
	dropbox-client-util.h \
	dropbox.c

libnautilus_dropbox_la_LDFLAGS = -module -avoid-version
libnautilus_dropbox_la_LIBADD  = $(NAUTILUS_LIBS) $(GLIB_LIBS) $(LIBNOTIFY_LIBS)
