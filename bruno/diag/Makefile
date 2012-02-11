CC		:= $(CROSS_COMPILE)gcc
AR		:= $(CROSS_COMPILE)ar
RANLIB		:= $(CROSS_COMPILE)ranlib

CFLAGS		+= -Os -Wall -pipe -fno-strict-aliasing -I./include
LIBS		:= -lmocactl -lmoca -lpthread -lm
SRCS		:= diagd.c diagTest.c
OBJS		:= diagd diagTest

# It is the diag core files
CORE_OBJS	:= diagParseKernMsgs.o diagMonApis.o diagNetworkTests.o diagApis.o diagLogging.o diagSubs.o diagMoca.o diagParseRefData.o


.PHONY: all
all: $(OBJS)

.PHONY: clean
clean:
	rm -f $(OBJS) *.[od]

diagd: diagd.o $(CORE_OBJS)
	$(CC) $(LDFLAGS) diagd.o $(CORE_OBJS) -o $@ $(LIBS)

diagTest: diagTest.o $(CORE_OBJS)
	$(CC) $(LDFLAGS) diagTest.o $(CORE_OBJS) -o $@ $(LIBS)

ifneq ($(MAKECMDGOALS),clean)
-include $(SRCS:.c=.d)
endif
