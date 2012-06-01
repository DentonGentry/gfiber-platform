default:

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin

all:

install:
	mkdir -p $(BINDIR)
	cp ginstall.py $(BINDIR)
	ln -sf ginstall.py $(BINDIR)/ginstall

test:
	set -e; \
	for d in $(wildcard *_test.py); do \
		./$$d; \
	done

clean:
	rm -f *~
