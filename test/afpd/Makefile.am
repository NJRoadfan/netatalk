# Makefile.am for test/afpd/

pkgconfdir = @PKGCONFDIR@

TESTS = test.sh test

check_PROGRAMS = test
EXTRA_DIST = test.sh test.h subtests.h afpfunc_helpers.h
CLEANFILES = test.default test.conf

test_SOURCES =  test.c subtests.c afpfunc_helpers.c \
				$(top_srcdir)/etc/afpd/afp_config.c \
				$(top_srcdir)/etc/afpd/afp_dsi.c \
				$(top_srcdir)/etc/afpd/afp_options.c \
				$(top_srcdir)/etc/afpd/afp_util.c \
				$(top_srcdir)/etc/afpd/afprun.c \
				$(top_srcdir)/etc/afpd/appl.c \
				$(top_srcdir)/etc/afpd/auth.c \
				$(top_srcdir)/etc/afpd/catsearch.c \
				$(top_srcdir)/etc/afpd/desktop.c \
				$(top_srcdir)/etc/afpd/dircache.c \
				$(top_srcdir)/etc/afpd/directory.c \
				$(top_srcdir)/etc/afpd/enumerate.c \
				$(top_srcdir)/etc/afpd/extattrs.c \
				$(top_srcdir)/etc/afpd/fce_api.c \
				$(top_srcdir)/etc/afpd/fce_util.c \
				$(top_srcdir)/etc/afpd/file.c \
				$(top_srcdir)/etc/afpd/filedir.c \
				$(top_srcdir)/etc/afpd/fork.c \
				$(top_srcdir)/etc/afpd/hash.c \
				$(top_srcdir)/etc/afpd/mangle.c \
				$(top_srcdir)/etc/afpd/messages.c \
				$(top_srcdir)/etc/afpd/nfsquota.c \
				$(top_srcdir)/etc/afpd/ofork.c \
				$(top_srcdir)/etc/afpd/quota.c \
				$(top_srcdir)/etc/afpd/status.c \
				$(top_srcdir)/etc/afpd/switch.c \
				$(top_srcdir)/etc/afpd/uam.c \
				$(top_srcdir)/etc/afpd/unix.c \
				$(top_srcdir)/etc/afpd/volume.c

test_CFLAGS = \
	-I$(top_srcdir)/etc/afpd \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/sys \
	@GSSAPI_CFLAGS@ @KRB5_CFLAGS@ @QUOTA_CFLAGS@\
	-DAPPLCNAME \
	-DSERVERTEXT=\"$(SERVERTEXT)/\" \
	-D_PATH_AFPDPWFILE=\"$(pkgconfdir)/afppasswd\" \
	-D_PATH_AFPDUAMPATH=\"$(UAMS_PATH)/\" \
	-D_PATH_CONFDIR=\"$(pkgconfdir)/\" \
	-D_PATH_STATEDIR='"$(localstatedir)/netatalk/"'

test_LDADD = \
	$(top_builddir)/libatalk/libatalk.la \
	@LIBGCRYPT_LIBS@ @QUOTA_LIBS@ @WRAP_LIBS@ @LIBADD_DL@ @ACL_LIBS@ @PTHREAD_LIBS@ @GSSAPI_LIBS@ @KRB5_LIBS@ @MYSQL_LIBS@

test_LDFLAGS = -export-dynamic

if HAVE_ACLS
test_SOURCES += $(top_srcdir)/etc/afpd/acls.c
endif

if WITH_SPOTLIGHT
test_SOURCES += $(top_srcdir)/etc/afpd/spotlight.c $(top_srcdir)/etc/afpd/spotlight_marshalling.c
test_LDADD += $(top_builddir)/etc/spotlight/libspotlight.la @TALLOC_LIBS@
test_CFLAGS += @TRACKER_CFLAGS@ @TALLOC_CFLAGS@
endif

if WITH_DTRACE
DTRACE_OBJ = $(top_builddir)/etc/afpd/test-afp_dsi.o \
             $(top_builddir)/etc/afpd/test-fork.o \
             $(top_builddir)/etc/afpd/test-appl.o \
             $(top_builddir)/etc/afpd/test-catsearch.o \
             $(top_builddir)/etc/afpd/test-directory.o \
             $(top_builddir)/etc/afpd/test-enumerate.o \
             $(top_builddir)/etc/afpd/test-file.o \
             $(top_builddir)/etc/afpd/test-filedir.o
afp_dtrace.o: $(top_srcdir)/include/atalk/afp_dtrace.d $(DTRACE_OBJ)
	if test -f afp_dtrace.o ; then rm -f afp_dtrace.o ; fi
	$(LIBTOOL) --mode=execute dtrace -G -s $(top_srcdir)/include/atalk/afp_dtrace.d -o afp_dtrace.o $(DTRACE_OBJ)
test_LDADD += afp_dtrace.o @DTRACE_LIBS@
CLEANFILES += afp_dtrace.o
endif
