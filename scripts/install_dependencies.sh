#!/bin/bash

# echo commands
set -x
# exit on error
set -e

# save folder of script for location of the patches
PATCH_FOLDER=$(readlink -f $(dirname $0))

# install system dependencies
sudo apt install git python haskell-stack flex bison libreadline-dev bc

# collect binaries in this folder
BIN_FOLDER_STR='$HOME/.local/bin'
BIN_FOLDER=$(envsubst <<< $BIN_FOLDER_STR)
mkdir -p $BIN_FOLDER
if ! grep "PATH.*$BIN_FOLDER_STR" ~/.bashrc >/dev/null; then
    echo "export PATH=\$PATH:$BIN_FOLDER_STR" >>~/.bashrc
fi

# build everything in temporary folder
TMP_FOLDER='/tmp'
cd $TMP_FOLDER

# install syfco
git clone https://github.com/meyerphi/syfco.git
cd syfco
stack setup
stack build
stack install
cd ..
rm -rf syfco

# install combine-aiger
git clone https://github.com/meyerphi/combine-aiger.git
cd combine-aiger
make
cp combine-aiger $BIN_FOLDER
cd ..
rm -rf combine-aiger

# install NuSMV
wget http://nusmv.fbk.eu/distrib/NuSMV-2.6.0.tar.gz
tar -xzf NuSMV-2.6.0.tar.gz
patch -p0 <$PATCH_FOLDER/nusmv_minisat.patch
patch -p0 <$PATCH_FOLDER/nusmv_cudd.patch
patch -p0 <$PATCH_FOLDER/nusmv_cmake.patch
cd NuSMV-2.6.0/NuSMV
mkdir build
cd build
cmake ..
make
cp bin/ltl2smv $BIN_FOLDER
cd ../../..
rm -r NuSMV-2.6.0.tar.gz NuSMV-2.6.0

# install nuXmv
wget https://es-static.fbk.eu/tools/nuxmv/downloads/nuXmv-2.0.0-linux64.tar.gz
tar -xzf nuXmv-2.0.0-linux64.tar.gz nuXmv-2.0.0-Linux/bin/nuXmv
cp nuXmv-2.0.0-Linux/bin/nuXmv $BIN_FOLDER
rm -r nuXmv-2.0.0-Linux nuXmv-2.0.0-linux64.tar.gz

# install aiger tools
wget http://fmv.jku.at/aiger/aiger-1.9.9.tar.gz
tar -xzf aiger-1.9.9.tar.gz
cd aiger-1.9.9
./configure.sh
make
cp smvtoaig $BIN_FOLDER
cd ..
rm -r aiger-1.9.9 aiger-1.9.9.tar.gz
