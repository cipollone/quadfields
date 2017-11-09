# Vrep plugin makefile

# vrep install directory
VREPDIR=/home/roberto/bin/V-REP

CXX=gcc
CXXFLAGS=-x c++ -std=c++11 -Wall -Wextra -O3 -fPIC
LDFLAGS=-lstdc++ -ldl -lcln -lginac

# all built files in the current dir
SOURCES=libv_repExtSymDeriv.cpp $(shell echo ./vrep/common/stack/*.cpp) $(shell echo ./vrep/common/*.cpp)
INCLUDES=libv_repExtSymDeriv.hpp
DESTEXE=repExtSymDeriv
DESTLIB=libv_repExtSymDeriv.so
OBJECTS=$(SOURCES:.cpp=.o)
INCLUDESDIR=-I./vrep/include/ -I./vrep/include/stack/
LIBSDIR=-L$(VREPDIR)

.PHONY: mkexe mklib compile clean

compile: mklib

mkexe: $(DESTEXE)

mklib: $(DESTLIB)

$(DESTEXE): $(OBJECTS)
	$(CXX) $(LIBSDIR) -o $(DESTEXE) $(OBJECTS) $(LDFLAGS)

$(DESTLIB): $(OBJECTS)
	$(CXX) -shared -Wl,-soname,$(DESTLIB) $(LIBSDIR) -o $(DESTLIB) $(OBJECTS) $(LDFLAGS)
	cp $(DESTLIB) $(VREPDIR)/

%.o: %.cpp $(INCLUDES)
	$(CXX) -c $(CXXFLAGS) $(INCLUDESDIR) -o $@ $<
	

clean:
	rm -f $(DESTEXE) $(DESTLIB) $(OBJECTS)