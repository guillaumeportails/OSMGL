
# Prod
CXXFLAGS = -DNDEBUG -g -O2
LDFLAGS  = -g

# Profiling
#CXXFLAGS = -pg -DNDEBUG -O2
#LDFLAGS  = -pg

# Full check
#CXXFLAGS = -g2 -O1
#LDFLAGS  = -g2

CC  = gcc

CFLAGS = -Wall
CXXFLAGS += -Wall -DHAS_PTHREAD -I/usr/local/include -I/usr/local/include/freetype2
LDFLAGS  += -L/usr/local/lib -lexpat -lbz2 -lpthread \
	    -lpsapi \
	    -Wl,-O -Wl,-static -Wl,--enable-auto-import

# Link avec la DLL Expat
#LDFLAGS  = -g /usr/local/lib/libexpat.a -Wl,-O -Wl,--enable-auto-import

all: testosm testgl

clean:; /bin/rm .deps *.o *.exe gmon.out gprof.out

testosm: testosm.o OSM.o Files.o rusage.o
	g++ -o $@ $+ $(LDFLAGS)

testgl: testgl.o OSM.o Files.o mGL.o osmRender.o Geo.o rusage.o
	g++ -o $@ $+ $(LDFLAGS) -lftgl -lglut32 -lglu32 -lopengl32 

.deps: *.cpp *.h
	@g++ --depend $(CXXFLAGS) $+ > .deps

rusage.o: rusage.c rusage.h

-include .deps

