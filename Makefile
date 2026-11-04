CC    = gcc
SRCS  = main.c banner.h banner.c
FLAGS = -Wall -Wextra -Werror

main: $(SRCS)
	$(CC) $(FLAGS) -o main $(SRCS)
