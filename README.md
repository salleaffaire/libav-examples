# Libav Samples

## Hello World

### Build

```
g++ -I../../ffmpeg_build/include -L../../ffmpeg_build/lib hello_world.cpp -lavcodec -lavformat -lavfilter -lavdevice -lswresample -lswscale -lavutil -lz -lx264 -lx265 -lopus -lfdk-aac -lvpx -lvorbisenc  -lvorbis  -pthread -lgnutls -lmp3lame -lX11 -lm  -lvdpau -lva -lva-drm -lva-x11 -o hello_world
```

## Transcoding

### Build

```
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