#!/bin/bash
source ./banner.sh

mkdir -p passwords
cd passwords
wget --no-check-certificate "https://kelvin.iac.rm.cnr.it/passwords.zip"
unzip passwords.zip
rm passwords.zip

cd ..

mkdir -p MODELs
cd MODELs
wget --no-check-certificate "https://kelvin.iac.rm.cnr.it/AdamsPreTrainedKerasModels/PasswordPro_SMALL.zip"
unzip PasswordPro_SMALL.zip
rm PasswordPro_SMALL.zip
