.PHONY: test libco-64.so

all: libco-64.so libco-test-64

test: libco-64.so all
	@echo "==== TEST coroutine runtime ===="
	@LD_LIBRARY_PATH=./ ./libco-test-64

libco-test-64: main.c

libco-64.so: ../co.c ../co.h
	gcc -fPIC -shared -U_FORTIFY_SOURCE -O2 -std=gnu11 -ggdb  -gdwarf-4 -Wall -Werror -Wno-unused-result -Wno-unused-value -Wno-unused-variable ../co.c -o $@

libco-test-64: main.c
	gcc -I.. -L. main.c   -ggdb  -gdwarf-4 -o libco-test-64 -lco-64

clean:
	rm -f libco-test-* libco-64.so
