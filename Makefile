CC    = gcc
SRCS  = main.c banner.h banner.c logger.h logger.c
FLAGS = -Wall -Wextra -Werror

.PHONY: all dynamic static run clean

all: static dynamic

dynamic: $(SRCS)
	$(CC) $(FLAGS) -o main_dyn $(SRCS)

static: $(SRCS)
	$(CC) $(FLAGS) -static -o main_static $(SRCS)

run: dynamic
	./main_dyn

clean:
	rm -f main_dyn main_static
