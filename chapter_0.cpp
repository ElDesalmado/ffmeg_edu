
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <cstdint>
#include <string>

#include <memory>
#include <type_traits>
#include <iostream>
#include <fstream>
#include <sstream>

#include <algorithm>

//////////////////////////////
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// print out the steps and errors
static void logging(const char *fmt, ...);

// save a frame into a .pgm file
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename);
//////////////////////////////


int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame);

void save_gray_frame(const uint8_t *buf, size_t wrap, size_t xSize, size_t ySize, const std::string &filename);

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("You need to specify a media file.\n");
        return -1;
    }

    AVFormatContext *pFormatContext = avformat_alloc_context();

    if (!pFormatContext)
    {
        std::cerr << "Failed to allocate format context" << std::endl;
        return -1;
    }

    const char *videoSample = "file:small_bunny_1080p_60fps.mp4";

    int error = avformat_open_input(&pFormatContext, videoSample, nullptr, nullptr);

    if (error)
    {
        std::cerr << "Failed to open video sample. ERROR: " << error << std::endl;
        avformat_free_context(pFormatContext);
        return -1;
    }

    std::cout << "Format: " << pFormatContext->iformat->long_name <<
              ", duration: " << pFormatContext->duration << " us" << std::endl;

    auto iterBegin = pFormatContext->streams,
            iterEnd = std::next(pFormatContext->streams, pFormatContext->nb_streams),
            found = std::find_if(pFormatContext->streams,
                                 std::next(pFormatContext->streams,
                                           pFormatContext->nb_streams),
                                 [](AVStream *pStream)
                                 {
                                     return pStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
                                 });

    if (found == iterEnd)
    {
        std::cerr << "Failed to find video codec" << std::endl;
        avformat_close_input(&pFormatContext);
        avformat_free_context(pFormatContext);
        return -1;
    }

    AVCodec *pVideoCodec = avcodec_find_decoder((*found)->codecpar->codec_id);
    if (!pVideoCodec)
    {
        std::cerr << "ERROR: unsupported codec" << std::endl;
        avformat_close_input(&pFormatContext);
        avformat_free_context(pFormatContext);
        return -1;
    }

    AVCodecContext *pCodecContext = avcodec_alloc_context3(pVideoCodec);
    error = avcodec_parameters_to_context(pCodecContext, (*found)->codecpar);
    if (error)
    {
        std::cerr << "Failed to copy codec params to codec context" << std::endl;

        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);
        avformat_free_context(pFormatContext);
        return -1;
    }

    error = avcodec_open2(pCodecContext, pVideoCodec, nullptr);
    if (error)
    {
        std::cerr << "Failed to open codec" << std::endl;

        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);
        avformat_free_context(pFormatContext);
        return -1;
    }

    AVFrame *pFrame = av_frame_alloc();
    AVPacket *pPacket = av_packet_alloc();
    if (!pFrame || !pPacket)
    {
        std::cerr << "Failed to allocate frame or packet" << std::endl;

        avcodec_close(pCodecContext);
        avcodec_free_context(&pCodecContext);
        avformat_close_input(&pFormatContext);
        avformat_free_context(pFormatContext);

        return -1;
    }

    error = 0;
    int packets_to_process = 8;

    while (!error && packets_to_process)
    {
        error = av_read_frame(pFormatContext, pPacket);

        // WTF, how is it even possible?
        if (pPacket->stream_index != (*found)->index)
        {
            std::cout << "Unexpected packet stream index: " <<
                      pPacket->stream_index << std::endl;
            continue;
        }
        error = decode_packet(pPacket, pCodecContext, pFrame);

        --packets_to_process;
        av_packet_unref(pPacket);
    }

    av_packet_free(&pPacket);
    avcodec_close(pCodecContext);
    avcodec_free_context(&pCodecContext);
    avformat_close_input(&pFormatContext);
    avformat_free_context(pFormatContext);

    return 0;
}

int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame)
{
    int error = avcodec_send_packet(pCodecContext, pPacket);

    if (error)
    {
        char errorStr[64]{};
        av_strerror(error, errorStr, sizeof(errorStr));
        std::cerr << "Error while sending packet to decoder: " <<
                  errorStr << std::endl;
        return error;
    }

//    while (error != AVERROR(EAGAIN) &&
//           error != AVERROR_EOF)
//    {
//        error = avcodec_receive_frame(pCodecContext, pFrame);
//        if (error)
//        {
//            char errorStr[64]{};
//            av_strerror(error, errorStr, sizeof(errorStr));
//            std::cerr << "Error receiving frame: " << errorStr << std::endl;
//        }
    while (error >= 0)
    {
        // Return decoded output data (into a frame) from a decoder
        // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c
        error = avcodec_receive_frame(pCodecContext, pFrame);
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF)
            break;
        else if (error < 0)
            return error;

        logging(
                "Frame %d (type=%c, size=%d bytes) pts %d key_frame %d [DTS %d]",
                pCodecContext->frame_number,
                av_get_picture_type_char(pFrame->pict_type),
                pFrame->pkt_size,
                pFrame->pts,
                pFrame->key_frame,
                pFrame->coded_picture_number);

        std::string fileFrameName{};
        std::stringstream ss{};

        ss << "frame-" << pCodecContext->frame_number << ".pgm";
        ss >> fileFrameName;

        save_gray_frame(pFrame->data[0],
                        pFrame->linesize[0],
                        pFrame->width, pFrame->height,
                        fileFrameName);
    }

    return 0;
}

void save_gray_frame(const uint8_t *buf, size_t wrap, size_t xSize, size_t ySize, const std::string &filename)
{
    std::fstream fileFrame{filename, std::ios::out};
    if (!fileFrame)
        std::cerr << "Failed to create file frame" << std::endl;
    fileFrame << "P5\n" << xSize << ' ' << ySize << "\n255\n" << std::endl;

    for (size_t i = 0; i != ySize; ++i)
        fileFrame.write(reinterpret_cast<const char *>(std::next(buf, i * wrap)), xSize);
}

static void logging(const char *fmt, ...)
{
    va_list args;
    fprintf(stderr, "LOG: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

