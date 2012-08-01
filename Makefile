default: all

DIRS=ginstall diag cmds sysmgr base antirollback

all:     $(addsuffix /all,$(DIRS))
test:    $(addsuffix /test,$(DIRS))
clean:   $(addsuffix /clean,$(DIRS))
install: $(addsuffix /install,$(DIRS))
install-libs: $(addsuffix /install-libs,$(DIRS))

sysmgr/all: base/all

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
