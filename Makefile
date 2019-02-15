CC	   := g++
CFLAGS := -Wall -Wextra -std=c++17 -ggdb

BINDIR  := bin
SRCDIR  := src
INCLUDE := 
LIB		:= /usr/local/lib/

LIBRARIES	:= -lhl++ -l:libboost_filesystem.a -l:libboost_system.a
EXECUTABLE	:= main


all: $(BINDIR)/$(EXECUTABLE)

run: clean all
	clear
	./$(BINDIR)/$(EXECUTABLE)

$(BINDIR)/$(EXECUTABLE): $(SRCDIR)/*.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -L$(LIB) $^ -o $@ $(LIBRARIES)

clean:
	-rm $(BINDIR)/*
