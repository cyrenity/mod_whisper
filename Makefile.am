include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_whisper

if HAVE_KS
if HAVE_WEBSOCKETS
mod_LTLIBRARIES = mod_whisper.la
mod_whisper_la_SOURCES  = mod_whisper.c websock_glue.c
mod_whisper_la_CFLAGS   = $(AM_CFLAGS) $(WEBSOCKETS_CFLAGS) $(KS_CFLAGS)
mod_whisper_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(KS_LIBS)
mod_whisper_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(WEBSOCKETS_LIBS)
endif
endif
