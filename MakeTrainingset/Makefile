HASHCAT_LEGACY_ROOT :=../hashcat_legacy
CC = g++
BUILDDIR := obj
TARGETDIR := bin
SRCDIR := src
LIBS = 
INCLUDE = -Iinclude/ -Isrc -I${HASHCAT_LEGACY_ROOT}/include
OPT = -lpthread -O3 -Wall

OBJECTS = 
AUXILIARY=${HASHCAT_LEGACY_ROOT}/rp.c ${HASHCAT_LEGACY_ROOT}/common.c ${HASHCAT_LEGACY_ROOT}/tsearch.c ${SRCDIR}/hashcat_utils.c #${SRCDIR}/wordlist.cpp ${SRCDIR}/utilities.cpp
all: $(TARGETDIR)/makeTrainSet

$(TARGETDIR)/makeTrainSet: makeTrainSet.cpp $(AUXILIARY) $(OBJECTS)
	mkdir -p $(TARGETDIR)
	$(CC)  -g -o $@ $^ $(LIBS) $(INCLUDE) $(OPT)



