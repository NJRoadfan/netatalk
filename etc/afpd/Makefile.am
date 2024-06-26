# Makefile.am for etc/afpd/

pkgconfdir = @PKGCONFDIR@
BUILT_SOURCES =
EXTRA_DIST = afpstats-service.xml afpstats_service_glue.h
CLEANFILES =
DISTCLEANFILES =

sbin_PROGRAMS = afpd

afpd_SOURCES = \
	afp_config.c \
	afp_dsi.c \
	afp_options.c \
	afp_util.c \
	afprun.c \
	appl.c \
	auth.c \
	catsearch.c \
	desktop.c \
	dircache.c \
	directory.c \
	enumerate.c \
	extattrs.c \
	fce_api.c \
	fce_util.c \
	file.c \
	filedir.c \
	fork.c \
	hash.c \
	main.c \
	mangle.c \
	messages.c  \
	nfsquota.c \
	ofork.c \
	quota.c \
	status.c \
	switch.c \
	uam.c \
	unix.c \
	volume.c


afpd_LDADD =  \
	$(top_builddir)/libatalk/libatalk.la \
	@LIBGCRYPT_LIBS@ @QUOTA_LIBS@ @WRAP_LIBS@ @LIBADD_DL@ @ACL_LIBS@ @PTHREAD_LIBS@ @GSSAPI_LIBS@ @KRB5_LIBS@ @MYSQL_LIBS@

afpd_LDFLAGS = -export-dynamic

afpd_CFLAGS = \
	@GSSAPI_CFLAGS@ @KRB5_CFLAGS@ @PTHREAD_CFLAGS@ @QUOTA_CFLAGS@\
	-DAPPLCNAME \
	-DSERVERTEXT=\"$(SERVERTEXT)/\" \
	-D_PATH_AFPDPWFILE=\"$(pkgconfdir)/afppasswd\" \
	-D_PATH_AFPDUAMPATH=\"$(UAMS_PATH)/\" \
	-D_PATH_CONFDIR=\"$(pkgconfdir)/\" \
	-D_PATH_STATEDIR='"$(localstatedir)/netatalk/"'

if WITH_SPOTLIGHT
afpd_SOURCES += spotlight.c spotlight_marshalling.c
afpd_LDADD += $(top_builddir)/etc/spotlight/libspotlight.la @TALLOC_LIBS@
afpd_CFLAGS += @TRACKER_CFLAGS@ @TALLOC_CFLAGS@
endif

if HAVE_ACLS
afpd_SOURCES += acls.c
endif

if HAVE_DBUS_GLIB
BUILT_SOURCES += afpstats_service_glue.h
DISTCLEANFILES += afpstats_service_glue.h

afpstats_service_glue.h: afpstats-service.xml
	$(LIBTOOL) --mode=execute \
		dbus-binding-tool \
			--prefix=afpstats_obj \
			--mode=glib-server \
			--output=afpstats_service_glue.h \
			$(top_srcdir)/etc/afpd/afpstats-service.xml

afpd_SOURCES += afpstats.c afpstats_obj.c
afpd_CFLAGS  += $(DBUS_CFLAGS) $(DBUS_GLIB_CFLAGS) $(DBUS_GTHREAD_CFLAGS) -DDBUS_COMPILATION
afpd_LDFLAGS += $(DBUS_LIBS) $(DBUS_GLIB_LIBS) $(DBUS_GTHREAD_LIBS) -ldbus-glib-1
endif

if WITH_DTRACE
DTRACE_OBJ = afpd-afp_dsi.o afpd-fork.o afpd-appl.o afpd-catsearch.o afpd-directory.o afpd-enumerate.o afpd-file.o afpd-filedir.o
afp_dtrace.o: $(top_srcdir)/include/atalk/afp_dtrace.d $(DTRACE_OBJ)
	if test -f afp_dtrace.o ; then rm -f afp_dtrace.o ; fi
	CC=$(CC) $(LIBTOOL) --mode=execute dtrace -G -s $(top_srcdir)/include/atalk/afp_dtrace.d -o afp_dtrace.o $(DTRACE_OBJ)
afpd_LDADD += afp_dtrace.o @DTRACE_LIBS@
CLEANFILES += afp_dtrace.o
endif

noinst_HEADERS = auth.h afp_config.h desktop.h directory.h fce_api_internal.h file.h \
	 filedir.h fork.h icon.h mangle.h misc.h status.h switch.h \
	 uam_auth.h unix.h volume.h hash.h acls.h acl_mappings.h extattrs.h \
	 dircache.h afpstats_obj.h afpstats.h
