CC    = gcc
SRCS  = main.c banner.h banner.c
FLAGS = -Wall -Wextra -Werror

all: static dynamic

dynamic: $(SRCS)
	$(CC) $(FLAGS) -o main_dyn $(SRCS)

static: $(SRCS)
	$(CC) $(FLAGS) -static -o main_static $(SRCS)

run: dynamic
	./main_dyn
