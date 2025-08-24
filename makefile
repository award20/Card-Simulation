# Minimal build: compile + link only, then remove .o files

CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS :=

# All .c under src/ and its immediate subdirs
SRCS := $(wildcard src/*.c src/*/*.c)
OBJS := $(SRCS:.c=.o)

# Output binary (auto .exe on Windows when using MinGW)
TARGET := CardSimulation

.PHONY: all clean distclean

# Cross-platform delete command for object files
ifeq ($(OS),Windows_NT)
  # Convert forward slashes to backslashes and quote each file
  WINOBJS  := $(foreach f,$(OBJS),"$(subst /,\,$(f))")
  DEL_OBJS := cmd /C del /Q /F $(WINOBJS)
else
  DEL_OBJS := rm -f $(OBJS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	-$(DEL_OBJS)

# Generic compile rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-$(DEL_OBJS)

distclean: clean
	-$(RM) $(TARGET) $(TARGET).exe 2>/dev/null || true
