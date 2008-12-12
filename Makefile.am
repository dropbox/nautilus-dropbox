AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

bin_SCRIPTS = dropbox
CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST = dropbox.in serializeimages.py

dropbox: dropbox.in serializeimages.py
	python serializeimages.py < dropbox.in > dropbox
	chmod +x dropbox

SUBDIRS = data src
