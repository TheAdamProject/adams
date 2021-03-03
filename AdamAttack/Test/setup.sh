#!/bin/bash

source ./banner.sh
NAME_ZIP="passwords.zip"

mkdir -p passwords
cd passwords
wget --no-check-certificate $PASSWORD_URL$NAME_ZIP -O $NAME_ZIP
unzip -o $NAME_ZIP
rm $NAME_ZIP

cd ..

mkdir -p MODELs
cd MODELs
wget --no-check-certificate "https://kelvin.iac.rm.cnr.it/AdamsPreTrainedKerasModels/PasswordPro_SMALL.zip"
unzip PasswordPro_SMALL.zip
rm PasswordPro_SMALL.zip
