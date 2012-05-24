SYSMGR_TOP ?= ${shell pwd}/..

all:
	@echo "------------------- Building PLATFORM SYSMGR ----------------------"
	$(MAKE) $(MAKE_OPTIONS) -C $(SYSMGR_TOP)/src/peripheral all
	$(MAKE) $(MAKE_OPTIONS) -C $(SYSMGR_TOP)/src/main all

test:
	@echo "------------------- Building PLATFORM SYSMGR Test -----------------"
	$(MAKE) $(MAKE_OPTIONS) -C $(SYSMGR_TOP)/utest all
