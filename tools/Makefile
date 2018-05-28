CXXFLAGS=-O2 -std=gnu++2a
LDFLAGS=-lboost_filesystem -lboost_system -lc
CC=g++
LD=ld

default: all

%.o: %.cpp
	${CC} ${CXXFLAGS} -c -o $@ $<

cbfs2ini: cbfs2ini.o
	${CC} ${LDFLAGS} -o $@ $<

elf2ini: elf2ini.o
	${CC} ${LDFLAGS} -o $@ $<

all: cbfs2ini elf2ini

clean:
	rm -f *.o cbfs2ini elf2ini
