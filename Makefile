SRC = $(shell find . -type f -name "*.c")
DST = $(patsubst %.c,%,$(SRC))

all: $(DST)

% : %.c
	gcc -o $@ $<
