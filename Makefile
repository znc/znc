CXX=g++
TARGET=linux-gnu
MACHINE=i686
CXXFLAGS=-s -O2 -fomit-frame-pointer -D_MODULES -Wall -D_GNU_SOURCE -DHAVE_LIBSSL
#CXXFLAGS=-Wall -ggdb -D_DEBUG -D_MODULES -D_GNU_SOURCE -DHAVE_LIBSSL
INCLUDES=
LIBS=-ldl -lssl -rdynamic

OBJS=main.o znc.o User.o IRCSock.o UserSock.o DCCBounce.o DCCSock.o Chan.o Nick.o Server.o Modules.o md5.o Buffer.o Utils.o
SRCS=main.cpp znc.cpp User.cpp IRCSock.cpp UserSock.cpp DCCBounce.cpp DCCSock.cpp Chan.cpp Nick.cpp Server.cpp Modules.cpp md5.cpp Buffer.cpp Utils.cpp

all: znc

depend::
	cat /dev/null >.depend
	g++ -M $(CXXFLAGS) $(SRCS) $(INCLUDES) >.depend

znc: $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(LIBS) $(OBJS)

znc-static: $(OBJS)
	$(CXX) $(CXXFLAGS) -static -o $@ $(INCLUDES) $(OBJS) $(LIBS)
	strip $@

clean:
	rm -rf znc znc-static *.o core core.*

include .depend

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $*.o $*.cpp
