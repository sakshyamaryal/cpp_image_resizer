#include "ImageResizer.h"
#include <iostream>
#include <fstream>

bool ImageResizer::resizeImage(const std::string &inputPath, const std::string &outputPath, Size size) {
    // Initialize FFmpeg libraries
    // av_register_all();
    // avcodec_register_all();
    avformat_network_init();

    AVFormatContext *formatContext = nullptr;
    if (avformat_open_input(&formatContext, inputPath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file: " << inputPath << std::endl;
        return false;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream info for: " << inputPath << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    // Find the first video stream
    AVCodecParameters *codecParameters = nullptr;
    for (unsigned int i = 0; i < formatContext->nb_streams; ++i) {
        codecParameters = formatContext->streams[i]->codecpar;
        if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            break;
        }
    }

    if (!codecParameters) {
        std::cerr << "Could not find a video stream in: " << inputPath << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    // Open the codec
    const AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!codec) {
        std::cerr << "Could not find codec for: " << inputPath << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Could not allocate codec context." << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        std::cerr << "Could not copy codec parameters to context." << std::endl;
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return false;
    }

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec." << std::endl;
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return false;
    }

    // Create an AVFrame for the input image
    AVFrame *frame = av_frame_alloc();
    AVFrame *scaledFrame = av_frame_alloc();
    if (!frame || !scaledFrame) {
        std::cerr << "Could not allocate frames." << std::endl;
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return false;
    }

    // Get the original width and height
    int originalWidth = codecContext->width;
    int originalHeight = codecContext->height;

    // Calculate new dimensions
    int newWidth = static_cast<int>(static_cast<int>(size) * originalWidth / 1024);
    int newHeight = static_cast<int>(static_cast<int>(size) * originalHeight / 1024);

    // Allocate memory for the scaled frame
    scaledFrame->format = codecContext->pix_fmt;
    scaledFrame->width = newWidth;
    scaledFrame->height = newHeight;
    av_frame_get_buffer(scaledFrame, 0);

    // Set up the scaling context
    SwsContext *swsContext = sws_getContext(originalWidth, originalHeight, codecContext->pix_fmt,
                                             newWidth, newHeight, codecContext->pix_fmt,
                                             SWS_BILINEAR, nullptr, nullptr, nullptr);

    // Read frames and resize
    AVPacket packet;
    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == 0) {
            avcodec_send_packet(codecContext, &packet);
            while (avcodec_receive_frame(codecContext, frame) >= 0) {
                sws_scale(swsContext, frame->data, frame->linesize, 0, originalHeight,
                          scaledFrame->data, scaledFrame->linesize);
                // Save the scaled frame to output (handle format based on output)
            }
        }
        av_packet_unref(&packet);
    }

    // Cleanup
    sws_freeContext(swsContext);
    av_frame_free(&scaledFrame);
    av_frame_free(&frame);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
    return true;
}