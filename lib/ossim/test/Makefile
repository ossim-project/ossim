# Makefile for libossim test programs
#
# Test programs link against the gRPC-based libossim library

CC := gcc
CXX := g++
CFLAGS := -std=gnu11 -Wall -O2 -I../..
CXXFLAGS := -std=c++17 -Wall -O2 -I../..
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)
LDFLAGS := -L.. -l:libossim.a $(GRPC_LDFLAGS) -lgrpc++_reflection -lstdc++ -lm

TEST_PROGS := test_socket_client

all: ../libossim.a $(TEST_PROGS)

# Build the library first
../libossim.a:
	@echo "Building libossim..."
	$(MAKE) -C ..

test_socket_client: test_socket_client.c ../libossim.a
	@echo "Building test client: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TEST_PROGS)

.PHONY: all clean
