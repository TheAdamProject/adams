HASHCAT_LEGACY_ROOT :=../hashcat_legacy
CC = g++
BUILDDIR := obj
TARGETDIR := bin
SRCDIR := src
LIBS = 
INCLUDE = -Iinclude/ -Isrc -I${HASHCAT_LEGACY_ROOT}/include
OPT = -O3 -lstdc++fs
MODELS_DIR="AdamsPreTrainedKerasModels"
GENERATED_BIG="generated_BIG"
GENERATED2_BIG="generated2_BIG"
PASSWORDPRO_BIG="PasswordPro_BIG"

FILE_NAME_ARRAY=( "model.h5" "params.txt" "rules.txt" )
#GENERATED_BIG_ID_ARRAY=( "1MAKEap47eAtbrgUGA1FBIii6fFVUj-I4" "1OJ0acJHNeUJS043xcmjT0jXafheow1Zs" "1EZyBOqW9edkSHOQgVL66WVPtw7lbe8Pr")

OBJECTS = $(BUILDDIR)/set.o $(BUILDDIR)/utilities.o
AUXILIARY=${HASHCAT_LEGACY_ROOT}/rp.c ${HASHCAT_LEGACY_ROOT}/common.c ${HASHCAT_LEGACY_ROOT}/tsearch.c ${SRCDIR}/adam_attack.cpp ${SRCDIR}/hashcat_utils.c
all: $(TARGETDIR)/AdamAttack ${TARGETDIR}/AdamAttackJustLog

$(TARGETDIR)/AdamAttack: ${SRCDIR}/adam_main.cpp $(AUXILIARY) $(OBJECTS)
	mkdir -p $(TARGETDIR)
	$(CC)  -g -o $@ $^ $(LIBS) $(INCLUDE) $(OPT) -DPRINT_TYPE=1

$(TARGETDIR)/AdamAttackJustLog: ${SRCDIR}/adam_main.cpp $(AUXILIARY) $(OBJECTS)
	mkdir -p $(TARGETDIR)
	$(CC)  -g -o $@ $^ $(LIBS) $(INCLUDE) $(OPT) -DPRINT_TYPE=2


$(BUILDDIR)/set.o: ${SRCDIR}/set.c
	mkdir -p  $(BUILDDIR)
	$(CC) -c -o $@ $(LIBS) $(INCLUDE) $^ $(OPT)
    
$(BUILDDIR)/utilities.o: ${SRCDIR}/utilities.cpp
	mkdir -p  $(BUILDDIR)
	$(CC) -g -c -o $@ $(LIBS) $(INCLUDE) $^ $(OPT)


#download_models:
#	# Create root directory
#	mkdir -p ${MODELS_DIR}
#	# Create directory for each model
#	mkdir -p ${MODELS_DIR}/${GENERATED_BIG}
#	mkdir -p ${MODELS_DIR}/${GENERATED2_BIG}
#	mkdir -p ${MODELS_DIR}/${PASSWORDPRO_BIG}
#	declare -A FILENAMES=( ["model.h5"]="model.h5" ["params.txt"]="params.txt" ["rules.txt"]="rules.txt") \
#	; for key in "$${!FILENAMES[@]}" ; do \
#		wget --no-check-certificate "https://kelvin.iac.rm.cnr.it/AdamsPreTrainedKerasModels/${GENERATED_BIG}/$${FILENAMES[$${key}]}"  -O ${MODELS_DIR}/${GENERATED_BIG}/$${key} ;\
#	done 
#	declare -A FILENAMES=( ["model.h5"]="model.h5" ["params.txt"]="params.txt" ["rules.txt"]="rules.txt") \
#	; for key in "$${!FILENAMES[@]}" ; do \
#		wget --no-check-certificate "https://kelvin.iac.rm.cnr.it/AdamsPreTrainedKerasModels/${GENERATED2_BIG}/$${FILENAMES[$${key}]}"  -O ${MODELS_DIR}/${GENERATED2_BIG}/$${key} ;\
#	done 
#	declare -A FILENAMES=( ["model.h5"]="model.h5" ["params.txt"]="params.txt" ["rules.txt"]="rules.txt") \
#	; for key in "$${!FILENAMES[@]}" ; do \
#		wget --no-check-certificate "https://kelvin.iac.rm.cnr.it/AdamsPreTrainedKerasModels/${PASSWORDPRO_BIG}/$${FILENAMES[$${key}]}"  -O ${MODELS_DIR}/${PASSWORDPRO_BIG}/$${key} ;\
#	done 
