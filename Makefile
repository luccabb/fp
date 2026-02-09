PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -Wall -Wextra -pedantic -std=c99

fp: fp.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: install uninstall clean test

install: fp
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 fp $(DESTDIR)$(BINDIR)/fp

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/fp

clean:
	rm -f fp

test: fp
	./test.sh
