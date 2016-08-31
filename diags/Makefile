default: all

DIRS = common
ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfmn100)
 DIRS += windcharger
endif
ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gfsc100)
 DIRS += spacecast
endif
ifeq ($(BR2_TARGET_GENERIC_PLATFORM_NAME),gflt110)
 DIRS += chameleon
endif

PYTHON?=python

ifdef  CROSS_COMPILE
        CROSS_PREFIX:=$(CROSS_COMPILE)
else
        CROSS_PREFIX:=
endif

all:     $(addsuffix /all,$(DIRS))
test:    $(addsuffix /test,$(DIRS))
clean:   $(addsuffix /clean,$(DIRS))
install-libs: $(addsuffix /install-libs,$(DIRS))

windcharger/all spacecast/all:	common/all

# The install targets in the recursive call use setuptools to build the python
# packages. These cannot be run in parallel, as they appear to race with each
# other to write site-packages/easy-install.pth.
install:
	set -e; for d in $(DIRS); do echo "make: $$d"; $(MAKE) -C $$d install; done

%/all:
	$(MAKE) -C $* all

%/test:
	$(MAKE) -C $* test

%/clean:
	$(MAKE) -C $* clean

%/install-libs:
	$(MAKE) -C $* install-libs
