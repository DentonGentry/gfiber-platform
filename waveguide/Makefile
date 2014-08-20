
all:

%.test: %_test.py
	./$<

runtests: $(patsubst %_test.py,%.test,$(wildcard *_test.py))

test: all
	./wvtest/wvtestrun $(MAKE) runtests

clean:
	rm -rf *~ .*~ *.pyc
