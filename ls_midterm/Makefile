CC      ?= cc
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=

TARGET = ls
OBJECTS = main.o options.o listing.o format.o

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

main.o: main.c options.h listing.h format.h
options.o: options.c options.h
listing.o: listing.c listing.h options.h format.h
format.o: format.c format.h options.h

clean:
	rm -f $(TARGET) $(OBJECTS)

test: $(TARGET)
	python3 tests/test_ls.py ./$(TARGET)
