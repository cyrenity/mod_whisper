include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_whisper

if HAVE_KS

mod_LTLIBRARIES = mod_whisper.la
mod_whisper_la_SOURCES  = mod_whisper.c
mod_whisper_la_CFLAGS   = $(AM_CFLAGS) $(KS_CFLAGS)
mod_whisper_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(KS_LIBS)
mod_whisper_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

endif
