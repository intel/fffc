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

# Fix the broken clang install
echo "/usr/lib/clang/6.0/lib/linux" | sudo tee /etc/ld.so.conf.d/clang.conf
sudo ldconfig

# Install fffc
sudo ./setup.py install

# Run some tests
cd tests
./run_samples.sh

