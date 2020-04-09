INSTALLING
==========

Prerequisites
-------------
FFFC requires a plausibly modern x86_64 Linux environment to work. We make an
effort not to depend on anything newer than about 5 years, but just because a
feature landed in glibc 5 years ago doesn't mean that your 5 year old production
environment. In particular, FFFC is known to work on Ubuntu systems going back
to 16.04 and Debian going back to 9. Being more specific about its requirements:

* Linux kernel newer than 3.13.
* Python3 newer than 3.4
* GCC newer than 4.8 OR Clang newer than 3.6

Words of caution
----------------
FFFC is not intended as a way to analyze hostile binaries, and is 100%
guaranteed to cause you trouble if you do. Even well-intentioned binaries that
do things like write to disk or talk over the network can do substantial damage
to your system or network when being fuzzed.  Do not use it on production
systems, do not run it over adversarial input, and do not keep your backups on
the same machine you fuzz with. You have been warned!

Notes on the installations below
--------------------------------
These recipes are intended to get you to the point where things work well enough
to test, which means that in addition to the things which actually are critical
for FFFC's operation, they install both gcc and clang, as well as tools like
dwarfdump, git, etc. You can make do with a subset of these, but we install
everything here on the theory that disk space is cheap and time spent banging
your head against the wall only to later discover an obscure dependency is very
expensive.

Also, we add these as users report them. Please contribute recipes!

Debian >= 9 and Ubuntu >= 16.04
-----------------------------
FFFC is pretty easy to install on these systems. I've provided a simple demo
script below that should work if you've pulled FFFC down as a tarball-- just
update the version number as appropriate.

```bash
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
tar xvvf fffc.0.1.tar.gz
cd fffc
sudo ./setup.py install

# Run some tests
cd tests
./run_samples.sh
```

CentOS and RHEL
---------------
TODO

Docker
------
First build the container. Run the build from the root of the fffc source.

```console
$ docker build -t intelotc/fffc .
```

Then navigate to the directory you want to use within the container and run it
as follows, mounting in the current directory as `/usr/src/app`.

```console
$ docker run --security-opt seccomp=unconfined \
  -w /usr/src/app -v $PWD:/usr/src/app -u $(id -u):$(id -g) intelotc/fffc
```

You now have a shell in the container as your same user on the host (so you
don't mess up file permissions on the mounted target directory). You can omit
the `-u` flag for a root shell.
