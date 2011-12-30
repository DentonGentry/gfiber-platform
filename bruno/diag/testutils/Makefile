CROSS		:= mipsel-linux-
CC		:= $(CROSS)gcc
AR		:= $(CROSS)ar
RANLIB		:= $(CROSS)ranlib

CFLAGS		:= -Os -Wall -fno-strict-aliasing -I../../BCM_Moca_Src/include -I../../BCM_Moca_Src/include/dslcompat -I../include
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
