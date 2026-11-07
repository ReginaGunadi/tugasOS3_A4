CC    = gcc
SRCS  = main.c banner.c
HDRS  = banner.h
FLAGS = -Wall -Wextra -Werror

.PHONY: all dynamic static run clean

all: static dynamic

dynamic: $(SRCS) $(HDRS)
	$(CC) $(FLAGS) -o main_dyn $(SRCS)

static: $(SRCS) $(HDRS)
	$(CC) $(FLAGS) -static -o main_static $(SRCS)

run: dynamic
	./main_dyn

clean:
	rm -f main_dyn main_static
