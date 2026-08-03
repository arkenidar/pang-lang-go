# Pang C interpreter build
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -O2 -D_GNU_SOURCE
TARGET := pang
SRCS := main.c pang.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c pang.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	@echo "=== factorial (italian) ==="
	@echo "fattoriale 5" | ./$(TARGET) italian ../tests/fattoriale.parole -
	@echo ""
	@echo "=== factorial (english) ==="
	@echo "factorial 5" | ./$(TARGET) ../tests/factorial.words -