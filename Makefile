CC	   := g++
CFLAGS := -Wall -Wextra -std=c++17 -ggdb

BINDIR  := bin
SRCDIR  := src
INCLUDE := 
LIB		:= -L/usr/local/lib/

LIBRARIES	:= -lhl++ -lstdc++fs
EXECUTABLE	:= dedup


all: $(BINDIR)/$(EXECUTABLE)

run: clean all
	clear
	./$(BINDIR)/$(EXECUTABLE)

$(BINDIR)/$(EXECUTABLE): $(SRCDIR)/*.cpp
	$(CC) $(CFLAGS) $(INCLUDE) $(LIB) $^ -o $@ $(LIBRARIES)

clean:
	-rm $(BINDIR)/*
