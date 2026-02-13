CC := gcc
CFLAGS := -g -Wall -Iinclude
LDFLAGS :=

SRC_DIR := src
SOURCES := $(wildcard $(SRC_DIR)/*.c)
TARGET := disastrOS_test

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
