all: build

test:
	set -e; \
	for d in $(wildcard *_test.py); do \
		$(PYTHON) $$d; \
	done

PREFIX=/usr

build:
	PYTHONPATH=$(TARGETPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py build

clean:
	PYTHONPATH=$(TARGETPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py clean --all

install-libs: install

install:
	PYTHONPATH=$(TARGETPYTHONPATH) $(HOSTDIR)/usr/bin/python setup.py install --prefix=$(DESTDIR)$(PREFIX)
