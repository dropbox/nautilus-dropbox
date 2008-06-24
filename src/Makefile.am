INCLUDES =						\
	-DDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(WARN_CFLAGS)                                  \
	-Wall                                           \
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(NAUTILUS_CFLAGS)

nautilus_extensiondir=$(NAUTILUS_EXTENSION_DIR)

nautilus_extension_LTLIBRARIES=libnautilus-dropbox.la

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
	busy2.h busy.h idle.h \
	async-http-downloader.h \
	async-http-downloader.c \
	async-io-coroutine.h \
	dropbox.c

libnautilus_dropbox_la_LDFLAGS = -module -avoid-version
libnautilus_dropbox_la_LIBADD  = $(NAUTILUS_LIBS)
