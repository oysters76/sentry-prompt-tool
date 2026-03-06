# ──────────────────────────────────────────────────────────────────────
# sentry-tool – Makefile
# ──────────────────────────────────────────────────────────────────────

CC      := gcc
TARGET  := sentry-tool

SRCDIR  := src
SRCS    := $(SRCDIR)/main.c $(SRCDIR)/sentry_client.c
OBJS    := $(SRCS:.c=.o)

# pkg-config is the canonical way to get libcurl flags; fall back to
# hard-coded values if pkg-config is unavailable.
CURL_CFLAGS  := $(shell pkg-config --cflags libcurl  2>/dev/null)
CURL_LIBS    := $(shell pkg-config --libs   libcurl  2>/dev/null || echo -lcurl)

CJSON_CFLAGS := $(shell pkg-config --cflags libcjson 2>/dev/null)
CJSON_LIBS   := $(shell pkg-config --libs   libcjson 2>/dev/null || echo -lcjson)

CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -g \
           -D_POSIX_C_SOURCE=200809L \
           $(CURL_CFLAGS) $(CJSON_CFLAGS)
LDFLAGS := $(CURL_LIBS) $(CJSON_LIBS)

# ── Default target ────────────────────────────────────────────────────
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Convenience targets ───────────────────────────────────────────────
.PHONY: clean distclean install

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) $(TARGET)

# Install to /usr/local/bin (requires appropriate permissions)
PREFIX  ?= /usr/local
install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

# ── Dependency tracking (auto-generated .d files) ─────────────────────
-include $(OBJS:.o=.d)

%.d: %.c
	$(CC) $(CFLAGS) -MM -MF $@ $<
