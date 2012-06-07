default: all

DIRS=ginstall diag cmds base

all:     $(addsuffix /all,$(DIRS))
test:    $(addsuffix /test,$(DIRS))
clean:   $(addsuffix /clean,$(DIRS))
install: $(addsuffix /install,$(DIRS))
install-libs: $(addsuffix /install-libs,$(DIRS))

%/all:
	$(MAKE) -C $* all

%/test:
	$(MAKE) -C $* test

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install

%/install-libs:
	$(MAKE) -C $* install-libs
