CROSS_COMPILE	:=
CC		:= $(CROSS_COMPILE)gcc
AR		:= $(CROSS_COMPILE)ar
RANLIB		:= $(CROSS_COMPILE)ranlib

CFLAGS		:= -Os -Wall -fno-strict-aliasing -I../../moca/include -I../../moca/include/dslcompat -I../include -DDIAG_TEST_UTIL
#CFLAGS		:= -Os -Wall -fno-strict-aliasing -I../include
LDFLAGS		:= -L../Diag -L.

LIBS		:= -lpthread
MOCACTL_LIBS	:= $(LIBS) -lm

SRCS		:= diagTester.c
OBJS		:= diagTester         


.PHONY: all
all: $(OBJS)

.PHONY: clean
clean:
	rm -f $(OBJS) *.[od]

diagTester: diagTester.o
	$(CC) $(LDFLAGS) diagTester.o -o $@ $(MOCACTL_LIBS)


ifneq ($(MAKECMDGOALS),clean)
-include $(SRCS:.c=.d)
endif
