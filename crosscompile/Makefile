# Makefile for the POSIX (Linux/other Unix) build of oras
#
# This tree reuses the platform-independent oras sources via symlinks into
# ../c and ../h (see c/ and include/), and supplies POSIX-specific
# replacements for the RISC OS-only pieces (os.h veneer and the URL_Fetcher
# transport) from posix/.
#
# Requires: gcc (or another C compiler), libcurl development headers
# (libcurl4-openssl-dev or equivalent) and cJSON development headers
# (libcjson-dev), discovered via pkg-config.
#
# A fully static build is not currently possible here: while libcurl.a
# exists, several of its static transitive dependencies (nghttp2, librtmp,
# libssh, libpsl, libgssapi_krb5, libldap) are not installed as static
# libraries, and libcjson ships no static archive at all. This Makefile
# therefore links dynamically. If a static build is required, install the
# missing -dev/static packages and see the 'static' target below.

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2 -g
CPPFLAGS := -Iinclude -Iposix $(shell pkg-config --cflags libcurl)
LDLIBS   := $(shell pkg-config --libs libcurl libcjson)

# Sources say #include "cJSON/cJSON.h"; the system cJSON package installs
# just cJSON.h in its own include directory (no "cJSON" subdirectory), so
# a symlink is created at build time - from wherever pkg-config says the
# system header actually lives - rather than committed, since that
# location varies by distribution.
CJSON_DIR := $(patsubst -I%,%,$(shell pkg-config --cflags-only-I libcjson))

COMMON_OBJS = c/oras_types.o \
              c/oci_auth.o \
              c/oci_manifest.o \
              c/oci_registry.o \
              c/riscos_metadata.o \
              c/riscos_fileset.o \
              c/sha256.o \
              posix/os_posix.o \
              posix/url_fetch_posix.o

.PHONY: all clean test static

all: oras oras-selftest

oras: c/main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

oras-selftest: c/selftest.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

include/cJSON/cJSON.h:
	mkdir -p include/cJSON
	ln -sfn $(CJSON_DIR)/cJSON.h include/cJSON/cJSON.h

%.o: %.c | include/cJSON/cJSON.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

test: oras-selftest
	./oras-selftest

# Best-effort static link. Only works if the static archives for libcurl's
# transitive dependencies are all present; install them first (e.g.
# libnghttp2-dev, librtmp-dev, libssh-dev, libpsl-dev, libkrb5-dev,
# libldap-dev) and note that libcjson has no static archive in Debian/
# Ubuntu, so a static cJSON.a would need to be built separately.
static: LDLIBS := $(shell pkg-config --static --libs libcurl libcjson)
static: CFLAGS += -static
static: all

clean:
	rm -f oras oras-selftest c/*.o posix/*.o
	rm -rf include/cJSON
