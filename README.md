# mod_whisper

A FreeSWITCH module to interface to your speech recognition & TTS server over websockets

## How to setup

1. Clone this repository in `${FREESWITCH_SOURCE_ROOT}/src/mod/asr_tts/` directory 

2. Add `src/mod/asr_tts/mod_whisper/Makefile` in `configure.ac` under `AC_CONFIG_FILES` section

2.1. Add following snippet in `configure.ac` 

```
PKG_CHECK_MODULES([WEBSOCKETS], [libwebsockets >= 0.0.1],[
  AM_CONDITIONAL([HAVE_WEBSOCKETS],[true])],[
  AC_MSG_RESULT([no]); AM_CONDITIONAL([HAVE_WEBSOCKETS],[false])])
```
3. Add the following two module in the ${FREESWITCH_SOURCE_ROOT}/modules.conf
```
asr_tts/mod_whisper
```

4. Run `autoreconf -f` before compiling freeswitch

5. Re-compile and install the freeswitch to install mod_whisper module.


6. Active the `mod_whisper` by add the following to lines into the `${FREESWITCH_INSTALLATION_ROOT}/conf/autoload_configs/modules.conf.xml`
<load module="mod_whisper"/>

7. Copy the lua script under `{FREESWITCH_INSTALLATION_ROOT}/scripts/`

8. Bind a number to build application by adding the following xml settings to the `${FREESWITCH_INSTALLATION_ROOT}/conf/dialplan/default.xml`
