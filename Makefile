PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man
CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -Wall -Wextra -pedantic -std=c99

fp: fp.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: install uninstall clean test

install: fp
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 fp $(DESTDIR)$(BINDIR)/fp
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 fp.1 $(DESTDIR)$(MANDIR)/man1/fp.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/fp
	rm -f $(DESTDIR)$(MANDIR)/man1/fp.1

clean:
	rm -f fp

test: fp
	./test.sh
