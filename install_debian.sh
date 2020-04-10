#! /bin/sh

# Get all of our dependencies from apt
sudo apt update
sudo apt install python3 build-essential git cmake python3-setuptools yasm dwarfdump clang

# Install subhook
git clone https://github.com/Zeex/subhook.git
cd subhook
cmake .
sudo make install
cd ..
sudo ldconfig

# Install fffc
cd fffc
sudo ./setup.py install

# Run some tests
cd tests
./run_samples.sh

