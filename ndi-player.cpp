/*
 * http://ffmpeg.org/doxygen/trunk/index.html
 *
 * Main components
 *
 * Format (Container) - a wrapper, providing sync, metadata and muxing for the
 * streams. Stream - a continuous stream (audio or video) of data over time.
 * Codec - defines how data are enCOded (from Frame to Packet)
 *        and DECoded (from Packet to Frame).
 * Packet - are the data (kind of slices of the stream data) to be decoded as
 * raw frames. Frame - a decoded raw frame (to be encoded or filtered).
 */

#define DEBUG_PROFILING 0
#define DEBUG_GENERAL 0

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

#include <fcntl.h>  // For file control
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>  // For STDIN_FILENO and other POSIX functions

#include <chrono>
#include <iostream>
#include <thread>

#include "Processing.NDI.Lib.h"

// https://github.com/joncampbell123/composite-video-simulator/issues/5
#undef av_err2str
#define av_err2str(errnum)                                                 \
  av_make_error_string((char *)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), \
                       AV_ERROR_MAX_STRING_SIZE, errnum)

// print out the steps and errors
static void logging(const char *fmt, ...);
// decode packets into frames
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext,
                         AVFrame *pFrame, NDIlib_send_instance_t *pNDI_send,
                         int frameRateNum, int frameRateDen);
// save a frame into a .pgm file
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize,
                            char *filename);

static void avframe_to_ndi_video_frame_420_to_UYVY(
    AVFrame *pFrameAV, NDIlib_video_frame_v2_t *pFrameNDI, int frameRateNum,
    int frameRateDen);

// Function to set terminal to non-blocking mode
void enableNonBlockingInput() {
  termios t;
  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag &= ~ICANON;  // Disable canonical mode
  t.c_lflag &= ~ECHO;    // Disable echo
  tcsetattr(STDIN_FILENO, TCSANOW, &t);

  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);  // Set non-blocking
}

// Function to restore terminal settings
void restoreTerminalSettings() {
  termios t;
  tcgetattr(STDIN_FILENO, &t);
  t.c_lflag |= ICANON;  // Enable canonical mode
  t.c_lflag |= ECHO;    // Enable echo
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

std::string getEnvVar(std::string const &key) {
  char *val = getenv(key.c_str());
  return val == NULL ? std::string("") : std::string(val);
}

int main(int argc, const char *argv[]) {
  std::string inputFileName;
  if (argc < 2) {
    inputFileName = getEnvVar("NDIPLAYER_INPUT_FILE");
    if (inputFileName.empty()) {
      std::cerr << "You need to specify a media file." << std::endl;
      return -1;
    }
  } else {
    inputFileName = argv[1];
  }

  logging("initializing all the containers, codecs and protocols.");

  // AVFormatContext holds the header information from the format (Container)
  // Allocating memory for this component
  // http://ffmpeg.org/doxygen/trunk/structAVFormatContext.html
  AVFormatContext *pFormatContext = avformat_alloc_context();
  if (!pFormatContext) {
    logging("ERROR could not allocate memory for Format Context");
    return -1;
  }

  logging("opening the input file (%s) and loading format (container) header",
          inputFileName.c_str());
  // Open the file and read its header. The codecs are not opened.
  // The function arguments are:
  // AVFormatContext (the component we allocated memory for),
  // url (filename),
  // AVInputFormat (if you pass NULL it'll do the auto detect)
  // and AVDictionary (which are options to the demuxer)
  // http://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga31d601155e9035d5b0e7efedc894ee49
  if (avformat_open_input(&pFormatContext, inputFileName.c_str(), NULL, NULL) !=
      0) {
    logging("ERROR could not open the file");
    return -1;
  }

  // now we have access to some information about our file
  // since we read its header we can say what format (container) it's
  // and some other information related to the format itself.
  logging("format %s, duration %lld us, bit_rate %lld",
          pFormatContext->iformat->name, pFormatContext->duration,
          pFormatContext->bit_rate);

  logging("finding stream info from format");
  // read Packets from the Format to get stream information
  // this function populates pFormatContext->streams
  // (of size equals to pFormatContext->nb_streams)
  // the arguments are:
  // the AVFormatContext
  // and options contains options for codec corresponding to i-th stream.
  // On return each dictionary will be filled with options that were not
  // found.
  // https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#gad42172e27cddafb81096939783b157bb
  if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
    logging("ERROR could not get the stream info");
    return -1;
  }

  // the component that knows how to enCOde and DECode the stream
  // it's the codec (audio or video)
  // http://ffmpeg.org/doxygen/trunk/structAVCodec.html
  const AVCodec *pCodec = NULL;
  // this component describes the properties of a codec used by the stream i
  // https://ffmpeg.org/doxygen/trunk/structAVCodecParameters.html
  AVCodecParameters *pCodecParameters = NULL;
  int video_stream_index = -1;

  int frameRateNum = 0;
  int frameRateDen = 0;
  // loop though all the streams and print its main information
  for (int i = 0; i < pFormatContext->nb_streams; i++) {
    AVCodecParameters *pLocalCodecParameters = NULL;
    pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
    logging("AVStream->time_base before open coded %d/%d",
            pFormatContext->streams[i]->time_base.num,
            pFormatContext->streams[i]->time_base.den);
    logging("AVStream->r_frame_rate before open coded %d/%d",
            pFormatContext->streams[i]->r_frame_rate.num,
            pFormatContext->streams[i]->r_frame_rate.den);
    logging("AVStream->start_time %" PRId64,
            pFormatContext->streams[i]->start_time);
    logging("AVStream->duration %" PRId64,
            pFormatContext->streams[i]->duration);

    logging("finding the proper decoder (CODEC)");

    const AVCodec *pLocalCodec = NULL;

    // finds the registered decoder for a codec ID
    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga19a0ca553277f019dd5b0fec6e1f9dca
    pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);

    if (pLocalCodec == NULL) {
      logging("ERROR unsupported codec!");
      // In this example if the codec is not found we just skip it
      continue;
    }

    // when the stream is a video we store its index, codec parameters and
    // codec
    if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (video_stream_index == -1) {
        video_stream_index = i;
        pCodec = pLocalCodec;
        pCodecParameters = pLocalCodecParameters;
      }

      logging("Video Codec: resolution %d x %d", pLocalCodecParameters->width,
              pLocalCodecParameters->height);
      frameRateNum = pFormatContext->streams[i]->r_frame_rate.num;
      frameRateDen = pFormatContext->streams[i]->r_frame_rate.den;
    } else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
      logging("Audio Codec: %d channels, sample rate %d",
              pLocalCodecParameters->ch_layout.nb_channels,
              pLocalCodecParameters->sample_rate);
    }

    // print its name, id and bitrate
    logging("\tCodec %s ID %d bit_rate %lld", pLocalCodec->name,
            pLocalCodec->id, pLocalCodecParameters->bit_rate);
  }

  if (video_stream_index == -1) {
    logging("File %s does not contain a video stream!", argv[1]);
    return -1;
  }

  // https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html
  AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
  if (!pCodecContext) {
    logging("failed to allocated memory for AVCodecContext");
    return -1;
  }

  // Fill the codec context based on the values from the supplied codec
  // parameters
  // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
  if (avcodec_parameters_to_context(pCodecContext, pCodecParameters) < 0) {
    logging("failed to copy codec params to codec context");
    return -1;
  }

  // Initialize the AVCodecContext to use the given AVCodec.
  // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga11f785a188d7d9df71621001465b0f1d
  if (avcodec_open2(pCodecContext, pCodec, NULL) < 0) {
    logging("failed to open codec through avcodec_open2");
    return -1;
  }

  // https://ffmpeg.org/doxygen/trunk/structAVFrame.html
  AVFrame *pFrame = av_frame_alloc();
  if (!pFrame) {
    logging("failed to allocate memory for AVFrame");
    return -1;
  }
  // https://ffmpeg.org/doxygen/trunk/structAVPacket.html
  AVPacket *pPacket = av_packet_alloc();
  if (!pPacket) {
    logging("failed to allocate memory for AVPacket");
    return -1;
  }

  int response = 0;
  // int how_many_packets_to_process = 30 * 30;

  // Not required, but "correct" (see the SDK documentation.
  if (!NDIlib_initialize()) {
    logging("ERROR: Failed to initialize NDI");
    return 0;
  }

  // Create an NDI sender
  NDIlib_send_instance_t pNDI_send = NDIlib_send_create();
  if (!pNDI_send) {
    logging("ERROR: Failed to create NDI sender");
    return 0;
  }

  bool isKeyPressed = false;

  enableNonBlockingInput();
  while (!isKeyPressed) {
    // fill the Packet with data from the Stream
    // https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga4fdb3084415a82e3810de6ee60e46a61
    while ((av_read_frame(pFormatContext, pPacket) >= 0) && (!isKeyPressed)) {
      // if it's the video stream
      if (pPacket->stream_index == video_stream_index) {
#if DEBUG_GENERAL == 1
        logging("AVPacket->pts %" PRId64, pPacket->pts);
#endif
        response = decode_packet(pPacket, pCodecContext, pFrame, &pNDI_send,
                                 frameRateNum, frameRateDen);
        if (response < 0) break;

        // Check if a key was pressed
        char ch;
        if (read(STDIN_FILENO, &ch, 1) > 0) {
          if (ch == 'q') isKeyPressed = true;
          break;  // Exit the loop if any key is pressed
        }
        // stop it, otherwise we'll be saving hundreds of frames
        // if (--how_many_packets_to_process <= 0) break;
      }
      // https://ffmpeg.org/doxygen/trunk/group__lavc__packet.html#ga63d5a489b419bd5d45cfd09091cbcbc2
      av_packet_unref(pPacket);
    }

    // Seek to the beginning of the file
    av_seek_frame(pFormatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
  }
  restoreTerminalSettings();

  logging("releasing all the resources");

  avformat_close_input(&pFormatContext);
  av_packet_free(&pPacket);
  av_frame_free(&pFrame);
  avcodec_free_context(&pCodecContext);

  // Destroy the NDI sender
  NDIlib_send_destroy(pNDI_send);

  // Not required, but nice
  NDIlib_destroy();

  return 0;
}

static void logging(const char *fmt, ...) {
  va_list args;
  fprintf(stderr, "LOG: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}

std::chrono::_V2::system_clock::time_point start;
std::chrono::_V2::system_clock::time_point end;

static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext,
                         AVFrame *pFrame, NDIlib_send_instance_t *pNDI_send,
                         int frameRateNum, int frameRateDen) {
  // First frame flag
  static bool firstFrame = true;

  // Supply raw packet data as input to a decoder
  // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga58bc4bf1e0ac59e27362597e467efff3
  int response = avcodec_send_packet(pCodecContext, pPacket);

  if (response < 0) {
    logging("Error while sending a packet to the decoder: %s",
            av_err2str(response));
    return response;
  }

  int framePerPacket = 0;
  while (response >= 0) {
    // Return decoded output data (into a frame) from a decoder
    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c
    response = avcodec_receive_frame(pCodecContext, pFrame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      logging("Error while receiving a frame from the decoder: %s",
              av_err2str(response));
      return response;
    }

    if (response >= 0) {
      logging(
          "Frame %d (type=%c, size=%d bytes, format=%d) pts %d key_frame %d "
          "[DTS %d]",
          pCodecContext->frame_num, av_get_picture_type_char(pFrame->pict_type),
          pFrame->pkt_size, pFrame->format, pFrame->pts, pFrame->key_frame);

      char frame_filename[1024];
      snprintf(frame_filename, sizeof(frame_filename), "%s-%ld.pgm", "frame",
               pCodecContext->frame_num);
      // Check if the frame is a planar YUV 4:2:0, 12bpp
      // That is the format of the provided .mp4 file
      // If it's not the NDI image might look funny
      if (pFrame->format != AV_PIX_FMT_YUV420P) {
        logging(
            "Warning: the generated NDI stream might look funny. We are "
            "expecting planar YUV 4:2:0 and we received something else.");
      }
      // save a grayscale frame into a .pgm file
      // save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width,
      //                 pFrame->height, frame_filename);

      // We are going to create a 1920x1080 interlaced frame at 29.97Hz.
      NDIlib_video_frame_v2_t NDI_video_frame;

      avframe_to_ndi_video_frame_420_to_UYVY(pFrame, &NDI_video_frame,
                                             frameRateNum, frameRateDen);

      // Send the frame
      NDIlib_send_send_video_v2(*pNDI_send, &NDI_video_frame);

      auto frameDuration =
          std::chrono::microseconds(1000000 / (frameRateNum / frameRateDen));

#if DEBUG_PROFILING == 1
      logging("Frame rate: %lds", frameDuration.count());
#endif

      // Get the current time
      // This is the end point before a sleep
      end = std::chrono::high_resolution_clock::now();

      // Calculate the time it took to process the frame
      auto duration =
          std::chrono::duration_cast<std::chrono::microseconds>(end - start);

      if (!firstFrame) {
#if DEBUG_PROFILING == 1
        // Logging the time it took to process the frame
        logging("Time to process frame: %ldus", duration.count());
#endif
      } else {
        duration = std::chrono::microseconds(0);
      }

      if (duration.count() > frameDuration.count()) {
        logging(
            "Warning: the frame processing time is longer than a frame "
            "durarion. Your output frame rate won't follow the source frame "
            "rate %ldus",
            duration.count());
      }

      // Wait for the remaining time
      std::this_thread::sleep_for(
          std::chrono::microseconds(frameDuration - duration));

      // Get the current time
      // This is the starting point after a sleep
      start = std::chrono::high_resolution_clock::now();

      // Free the video frame
      free(NDI_video_frame.p_data);

      // Update first frame flag
      firstFrame = false;

      // Increment the frame count
      framePerPacket++;
    }
  }

#if DEBUG_GENERAL == 1
  logging("Frames per packet: %d", framePerPacket);
#endif

  return 0;
}

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize,
                            char *filename) {
  FILE *f;
  int i;
  f = fopen(filename, "w");
  // writing the minimal required header for a pgm file format
  // portable graymap format ->
  // https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
  fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

  // writing line by line
  for (i = 0; i < ysize; i++) fwrite(buf + i * wrap, 1, xsize, f);
  fclose(f);
}

static void avframe_to_ndi_video_frame_420_to_UYVY(
    AVFrame *pFrameAV, NDIlib_video_frame_v2_t *pFrameNDI, int frameRateNum,
    int frameRateDen) {
  // Set the resolution of the frame
  pFrameNDI->xres = pFrameAV->width;
  pFrameNDI->yres = pFrameAV->height;

  // Set the stride
  pFrameNDI->line_stride_in_bytes = pFrameAV->linesize[0] * 2;

  // Set the frame rate of the frame
  pFrameNDI->frame_rate_N = frameRateNum;
  pFrameNDI->frame_rate_D = frameRateDen;

  // Set the picture aspect ratio of the frame
  pFrameNDI->picture_aspect_ratio = (float)pFrameAV->width / pFrameAV->height;

  // Set the frame format of the frame
  pFrameNDI->FourCC = NDIlib_FourCC_type_UYVY;

  // Set the timecode of the frame
  pFrameNDI->timecode = NDIlib_send_timecode_synthesize;

  // Allocate the memory
  pFrameNDI->p_data = (uint8_t *)malloc(pFrameAV->width * pFrameAV->height * 2);

#if DEBUG_PROFILING == 1
  // Capture the start time of the color conversion
  auto start = std::chrono::high_resolution_clock::now();
#endif

  // Set the data of the frame
  // For each line
  for (int line = 0; line < pFrameAV->height; line++) {
    // 2 Bytes per pixel
    uint8_t *dataDest =
        pFrameNDI->p_data + (line * pFrameNDI->line_stride_in_bytes);
    uint8_t *dataSourceY = pFrameAV->data[0] + line * pFrameAV->linesize[0];
    // 420 to 422, we repeat each chroma line twice
    uint8_t *dataSourceU =
        pFrameAV->data[1] + (line / 2) * pFrameAV->linesize[1];
    uint8_t *dataSourceV =
        pFrameAV->data[2] + (line / 2) * pFrameAV->linesize[2];

    // For each 2 pixels
    for (int pixel = 0; pixel < pFrameAV->width; pixel += 2) {
      // Set the first Y value of the pixel
      *dataDest++ = *dataSourceU++;
      *dataDest++ = *dataSourceY++;
      *dataDest++ = *dataSourceV++;
      *dataDest++ = *dataSourceY++;
    }
  }

#if DEBUG_PROFILING == 1
  // Capture the end time of the color conversion
  auto end = std::chrono::high_resolution_clock::now();

  // Calculate the time it took to convert the color
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // Logging the time it took to convert the color
  logging("Time to convert color: %ldus", duration.count());
#endif

  // Set the metadata of the frame
  pFrameNDI->p_metadata = NULL;
}