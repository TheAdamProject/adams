# SET THESE:
CPPFLOW_HOME = #PUT PATH TO CppFlow HERE!
TENSOFLOW_HOME = #PUT PATH TO TesorFlow APIs HERE!
#########################################################################

CC = g++

HASHCAT_LEGACY_ROOT :=../hashcat_legacy
CPPFLOW_INC = ${CPPFLOW_HOME}/include
TENSOFLOW_LIB = ${TENSOFLOW_HOME}/lib
TENSOFLOW_INC = ${TENSOFLOW_HOME}/include

BUILDDIR := obj
TARGETDIR := bin
SRCDIR := src

LIBS = -L${TENSOFLOW_LIB} -ltensorflow
INCLUDE = -Iinclude/ -Isrc -I${HASHCAT_LEGACY_ROOT}/include -I${TENSOFLOW_INC} -I$(CPPFLOW_INC)
OPT = -O3 -std=c++17 -lpthread 

OBJECTS = $(BUILDDIR)/utilities.o $(BUILDDIR)/adaptive_model.o $(BUILDDIR)/wordlist.o #$(BUILDDIR)/set.o
AUXILIARY=${HASHCAT_LEGACY_ROOT}/rp.c ${HASHCAT_LEGACY_ROOT}/common.c ${HASHCAT_LEGACY_ROOT}/tsearch.c ${SRCDIR}/adam_attack.cpp ${SRCDIR}/hashcat_utils.c

all: $(TARGETDIR)/AdamAttack

$(TARGETDIR)/AdamAttack: adam_main.cpp $(AUXILIARY) $(OBJECTS)
	mkdir -p $(TARGETDIR)
	$(CC)  -g -o $@ $^ $(LIBS) $(INCLUDE) $(OPT)

$(BUILDDIR)/adaptive_model.o: ${SRCDIR}/adaptive_model.cpp
	mkdir -p  $(BUILDDIR)
	$(CC) -c -o $@  $(INCLUDE) $^ $(LIBS) $(OPT)
       
$(BUILDDIR)/utilities.o: ${SRCDIR}/utilities.cpp
	mkdir -p  $(BUILDDIR)
	$(CC) -g -c -o $@ $(LIBS) $(INCLUDE) $^ $(OPT)
    
$(BUILDDIR)/wordlist.o: ${SRCDIR}/wordlist.cpp
	mkdir -p  $(BUILDDIR)
	$(CC) -g -c -o $@ $(LIBS) $(INCLUDE) $^ $(OPT)

clean:
	rm obj/*
	rm bin/*
