FROM ubuntu:18.04

WORKDIR /usr/src

RUN apt-get update && \
  apt-get install -y \
    python3 \
    python3-pip \
    build-essential \
    git \
    gdb \
    cmake \
    python3-setuptools \
    yasm \
    dwarfdump \
    clang && \
  rm -rf /var/lib/apt/lists/* && \
  git clone --depth=1 https://github.com/Zeex/subhook.git && \
  cd subhook && \
  mkdir build && \
  cd build && \
  cmake .. -DCMAKE_INSTALL_PREFIX:PATH=/usr && \
  make -j $(($(nproc)*4)) && \
  make install && \
  ldconfig && \
  cd .. && \
  rm -rf subhook

COPY . /usr/src/fffc

RUN python3 -m pip install /usr/src/fffc
