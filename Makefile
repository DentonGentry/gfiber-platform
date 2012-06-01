default: all

DIRS=ginstall diag cmds

all:     $(addsuffix /all,$(DIRS))
test:    $(addsuffix /test,$(DIRS))
clean:   $(addsuffix /clean,$(DIRS))
install: $(addsuffix /install,$(DIRS))

%/all:
	$(MAKE) -C $* all

%/test:
	$(MAKE) -C $* test

%/clean:
	$(MAKE) -C $* clean

%/install:
	$(MAKE) -C $* install
