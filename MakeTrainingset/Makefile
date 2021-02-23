CC = mpic++
BUILDDIR := obj
TARGETDIR := bin
LIBS = 
HC = ../hashcat_legacy
INCLUDE = -I$(HC)/include/ -Isrc

OPT = -O3

OBJECTS = $(BUILDDIR)/set.o

all: $(TARGETDIR)/makeTrainingsetMPI 
    
$(TARGETDIR)/makeTrainingsetMPI: makeTrainingsetMPI.c $(OBJECTS)
	mkdir -p $(TARGETDIR)
	$(CC) -g -o $@ $^ $(LIBS) $(HC)/rp.c $(HC)/common.c $(HC)/tsearch.c $(INCLUDE) $(OPT)
    
$(BUILDDIR)/set.o: src/set.c
	mkdir -p $(BUILDDIR)
	$(CC) -c -o $@ $(LIBS) $(INCLUDE) $^ $(OPT)
