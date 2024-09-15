FROM ubuntu:22.04

WORKDIR /root

RUN apt-get update -qq && apt-get -y install \
  autoconf \
  automake \
  build-essential \
  cmake \
  git-core \
  libass-dev \
  libfreetype6-dev \
  libgnutls28-dev \
  libmp3lame-dev \
  libsdl2-dev \
  libtool \
  libva-dev \
  libvdpau-dev \
  libvorbis-dev \
  libxcb1-dev \
  libxcb-shm0-dev \
  libxcb-xfixes0-dev \
  meson \
  ninja-build \
  pkg-config \
  texinfo \
  wget \
  yasm \
  zlib1g-dev

RUN apt install -y libunistring-dev

RUN apt-get install -y nasm
RUN apt-get install -y libx264-dev
RUN apt-get install -y libvpx-dev
RUN apt-get install -y libfdk-aac-dev
RUN apt-get install -y libopus-dev
RUN apt-get install -y libdav1d-dev
RUN apt-get install -y libx265-dev libnuma-dev

COPY NDI_SDK/ /root/NDI_SDK/
COPY ffmpeg-snapshot.tar.bz2 /root/ffmpeg_sources/ffmpeg-snapshot.tar.bz2
COPY fetch-build-install.sh /root/fetch-build-install.sh

RUN ./fetch-build-install.sh

RUN apt-get install -y libavahi-client3
RUN apt install -y libncurses5-dev
RUN apt-get install -y liblzma-dev

COPY file_example_MP4_1920_18MG.mp4 /root/file_example_MP4_1920_18MG.mp4
COPY file_example_AVI_1920_2_3MG.avi /root/file_example_AVI_1920_2_3MG.avi

ENV LD_LIBRARY_PATH=/root/NDI_SDK/lib/x86_64-linux-gnu

COPY ndi-player.cpp /root/ndi-player.cpp

RUN g++ -I./ffmpeg_build/include -I./NDI_SDK/include -L ./NDI_SDK/lib/x86_64-linux-gnu -L./ffmpeg_build/lib ndi-player.cpp -lavformat -lndi -lavcodec -lavfilter -lavdevice -lswresample -lswscale -lavutil -lz -llzma -lx264 -lx265 -lopus -lfdk-aac -lvpx -lvorbisenc -lvorbis  -ldrm -pthread -lgnutls -lmp3lame -lX11 -lm  -lvdpau -lva -lva-drm -lva-x11 -lncurses -o ndi-player

CMD ["./ndi-player", "file_example_MP4_1920_18MG.mp4"]
