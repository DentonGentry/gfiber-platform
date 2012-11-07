CROSS_COMPILE	:=
CC		:= $(CROSS_COMPILE)gcc
AR		:= $(CROSS_COMPILE)ar
RANLIB		:= $(CROSS_COMPILE)ranlib

MOCADIR := ../../../../broadcom/drivers/moca/
CFLAGS	:= -Os -Wall -fno-strict-aliasing -I$(MOCADIR)/include -I$(MOCADIR)/lib -I../include -DDIAG_TEST_UTIL
#CFLAGS	:= -Os -Wall -fno-strict-aliasing -I../include
LDFLAGS	:= -L../Diag -L.

LIBS		:= -lpthread
MOCACTL_LIBS	:= $(LIBS) -lm

SRCS		:= diagTester.c diagMocaLog.c
OBJS		:= diagTester         


.PHONY: all
all: $(OBJS)

.PHONY: clean
clean:
	rm -f $(OBJS) *.[od]

diagTester: diagTester.o diagTestMocaLog.o
	$(CC) $(LDFLAGS) diagTester.o diagTestMocaLog.o -o $@ $(MOCACTL_LIBS)


ifneq ($(MAKECMDGOALS),clean)
-include $(SRCS:.c=.d)
endif
