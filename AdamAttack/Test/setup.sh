#!/bin/bash

source ./banner.sh
PASSWORD_URL="https://kelvin.iac.rm.cnr.it/"
NAME_ZIP="passwords.zip"
PASSWORDPRO_BIG="PasswordPro_BIG"
FILE_NAME_ARRAY=( "model.h5" "params.txt" "rules.txt" )

mkdir -p passwords
cd passwords
wget --no-check-certificate $PASSWORD_URL$NAME_ZIP -O $NAME_ZIP
unzip -o $NAME_ZIP
rm $NAME_ZIP

cd ..

mkdir -p MODELs
cd MODELs
mkdir -p ${PASSWORDPRO_BIG}
#declare -A FILENAMES=( ["model.h5"]="model.h5" ["params.txt"]="params.txt" ["rules.txt"]="rules.txt") \
FILENAMES=("model.h5" "params.txt" "rules.txt")
for key in "${FILENAMES[@]}"  
do 
    wget --no-check-certificate "https://kelvin.iac.rm.cnr.it/AdamsPreTrainedKerasModels/${PASSWORDPRO_BIG}/${key}" -O ${PASSWORDPRO_BIG}/${key} 
done 
