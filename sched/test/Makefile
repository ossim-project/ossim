# Makefile for scx_ossim test programs

CC := gcc
CFLAGS := -std=gnu11 -Wall -O2
LDFLAGS := -lpthread

TEST_PROGS := test_socket_client

all: $(TEST_PROGS)

test_socket_client: test_socket_client.c
	@echo "Building test client: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TEST_PROGS)

.PHONY: all clean
