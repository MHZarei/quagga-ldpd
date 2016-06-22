## Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = -I.. -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_builddir)/lib
DEFS = @DEFS@ -DSYSCONFDIR=\"$(sysconfdir)/\"
INSTALL_SDATA=@INSTALL@ -m 600
LIBS = @LIBS@

AM_CFLAGS = $(WERROR)

noinst_LIBRARIES = libldp.a
sbin_PROGRAMS = ldpd

libldp_a_SOURCES = \
	accept.c address.c adjacency.c control.c hello.c init.c interface.c \
	keepalive.c l2vpn.c labelmapping.c lde.c lde_lib.c ldpd.c \
	ldpe.c log.c neighbor.c notification.c packet.c pfkey.c \
	socket.c util.c ldp_vty_cmds.c ldp_vty_conf.c ldp_vty_exec.c \
	ldp_debug.c ldp_zebra.c

noinst_HEADERS = \
	control.h lde.h ldpd.h ldpe.h ldp.h log.h ldp_debug.h ldp_vty.h

ldpd_SOURCES = ldpd.c
ldpd_LDADD = libldp.a ../lib/libzebra.la @LIBCAP@ @ZEROMQ_LIBS@

examplesdir = $(exampledir)
dist_examples_DATA = ldpd.conf.sample

if HAVE_ZEROMQ
PYTHON2_BIN=@PYTHON2_BIN@
print-% : ; @echo $($*)

ldp.capnp-bin: ldp.capnp
	capnp compile -I../capnp -o- $^ > $@
ldpd.ndef-gen.c: ldpd.ndef ldp.capnp-bin
	+$(PYTHON2_BIN) ../capnp/schemafoo.py --cpp "`make -s --no-print-directory 'CC=$(CPP)' print-COMPILE`" $<
ldpd.ndef-gen.h: ldpd.ndef-gen.c
ldp.capnp-gen.c: ldpd.ndef-gen.c
ldp.capnp-gen.h: ldpd.ndef-gen.c
ldpd.o: ldpd.ndef-gen.h

CLEANFILES = ldpd.ndef-gen.c ldpd.ndef-gen.h ldp.capnp-bin ldp.capnp-gen.c ldp.capnp-gen.h

libldp_a_SOURCES += ldp.capnp-gen.c ldpd.ndef-gen.c
BUILD_SOURCE = ldp.capnp-gen.c ldpd.ndef-gen.c
EXTRA_DIST = ldpd.ndef ldpd.capnp
endif
