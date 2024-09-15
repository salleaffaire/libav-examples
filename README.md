# Libav Samples

```note
Built on Ununtu 22.04. 
```

To build the 2 examples, you must first install FFMPEG or build it frome source. 

Follow the [compilation guide](https://trac.ffmpeg.org/wiki/CompilationGuide).

1- [Get the dependencies](https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu#GettheDependencies)

```
sudo apt-get install nasm
sudo apt-get install libx264-dev
sudo apt-get install libvpx-dev
sudo apt-get install libfdk-aac-dev
sudo apt-get install libopus-dev
sudo apt-get install libdav1d-dev
sudo apt-get install libx265-dev libnuma-dev
```

1.5- You might need to run this after installing all the needed codecs.

```bash
sudo apt-get install libunistring-dev
```
see, [FFMPEG not building](https://askubuntu.com/questions/1252997/unable-to-compile-ffmpeg-on-ubuntu-20-04)

1.5.5-

In your home folder create a `ffmpeg_sources` folder and copy `ffmpeg-snapshot.tar.bz2` into it.

2- From your home ($HOME) directory, execute the `fetch-build-install.sh` script. For subsequent rebuilds, just run `build-install.sh`.


When built, the artifacts should be in the `ffmpeg_build` folder.

## Hello World

### Build

Make sure you properly set the location of `ffmpeg_build`.

```bash
g++ -I../../ffmpeg_build/include -L../../ffmpeg_build/lib hello-world.cpp -lavformat -lavcodec -lavfilter -lavdevice -lswresample -lswscale -lavutil -lz -llzma -lx264 -lx265 -lopus -lfdk-aac -lvpx -lvorbisenc -lvorbis  -ldrm -pthread -lgnutls -lmp3lame -lX11 -lm  -lvdpau -lva -lva-drm -lva-x11 -o hello-world
```

## NDI Player

### Build

```bash
g++ -I../../ffmpeg_build/include -I./NDI_SDK/include -L ../NDI_SDK/lib/x86_64-linux-gnu -L../../ffmpeg_build/lib ndi-player.cpp -lavformat -lndi -lavcodec -lavfilter -lavdevice -lswresample -lswscale -lavutil -lz -llzma -lx264 -lx265 -lopus -lfdk-aac -lvpx -lvorbisenc -lvorbis  -ldrm -pthread -lgnutls -lmp3lame -lX11 -lm  -lvdpau -lva -lva-drm -lva-x11 -lncurses -o ndi-player
```

## Transcoding

### Build

Make sure you properly set the location of `ffmpeg_build`.

```bash
g++ -I../../ffmpeg_build/include -L../../ffmpeg_build/lib video_debugging.cpp transcoding.cpp -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil -lz -lx264 -lx265 -lopus -lfdk-aac -lvpx -lvorbisenc  -lvorbis  -pthread -lgnutls -lmp3lame -lX11 -lm  -lvdpau -lva -lva-drm -lva-x11 -o transcoding
```


## Benchmarking with FFMPEG

To get the average time it takes to encode a video frame using FFmpeg, you can use the ffmpeg command along with the -benchmark option. The -benchmark option will print benchmarking information after the encoding process is complete, including the average time taken to encode a frame.

Here's how you can do it:

```bash
ffmpeg -i input_video.mp4 -c:v libx264 -benchmark -f null -
```

Let's break down the command:

ffmpeg: This is the FFmpeg command.
-i input_video.mp4: Specifies the input video file.
-c:v libx264: Specifies the video codec to use for encoding. In this example, it uses the H.264 (libx264) codec, but you can choose any other codec you prefer.
-benchmark: Enables benchmarking mode, which will print out statistics after the encoding process is finished.
-f null -: This part of the command redirects the output to null (i.e., no output file is created), as we're only interested in the benchmarking information, not the actual output file.
After the encoding process is complete, FFmpeg will display the benchmarking information, including the average time it took to encode a video frame. Look for the line starting with "bench" and find the value associated with "t" (time per frame).

Keep in mind that the actual time taken to encode a frame can vary depending on the video codec, the hardware used, and other encoding parameters. The reported value is an average across all frames during the encoding process.

## Docker

### Build

First, download the NDI SDK from their web site [NDI SDK](https://ndi.video/for-developers/ndi-sdk/).

Unzip it a place it at the top of the project folder under the name `NDI_SDK`.

