default:

PREFIX=/
BINDIR=$(DESTDIR)$(PREFIX)/bin
PYTHON?=python

all:

install:
	mkdir -p $(BINDIR)
	cp jsonpoll.py $(BINDIR)/jsonpoll

install-libs:
	@echo "No libs to install."

test:
	$(PYTHON) jsonpoll_test.py

clean:
	rm -rf *.pyc

lint:
	gpylint jsonpoll.py jsonpoll_test.py
