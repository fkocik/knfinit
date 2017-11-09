REVISION=$(shell git describe --tags --dirty=.dirty)
DATE=$(shell date +%s)

SRC=$(shell find ./ -iname '*.c' -type f)
OBJ=$(patsubst %.c,%.o,$(SRC))

.PHONY: clean

all: knfinit

knfinit: $(OBJ)
	gcc -o $@ $^

%.o: %.c
	gcc -DVERSION=$(REVISION) -DBUILD_DATE=$(DATE) -o $@ -c $^

clean:
	rm -f $(OBJ) knfinit

