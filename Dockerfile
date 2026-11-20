FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    fakeroot \
    gcc-13 g++-13 gcc-14 g++-14 \
    bc bison flex \
    libelf-dev libssl-dev \
    dwarves pahole zstd \
    kmod cpio xorriso mtools dosfstools \
    wget ca-certificates \
    automake autoconf m4 perl texinfo \
    mingw-w64 gnu-efi \
    && rm -rf /var/lib/apt/lists/*

# Дефолт — gcc-13 (стабильнее для NVIDIA), gcc-14 остаётся доступен как gcc-14
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

WORKDIR /build

CMD ["/bin/bash"]
