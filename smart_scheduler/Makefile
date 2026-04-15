CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude
SRCDIR   := src
BUILDDIR := build

SRCS := $(SRCDIR)/main.cpp \
        $(SRCDIR)/scheduler.cpp \
        $(SRCDIR)/metrics.cpp \
        $(SRCDIR)/io_utils.cpp

OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
TARGET := $(BUILDDIR)/scheduler

.PHONY: all clean

all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)
