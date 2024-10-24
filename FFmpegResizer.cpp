#include "FFmpegResizer.hpp"

FFmpegResizer::~FFmpegResizer() {
    cleanup();
}

bool FFmpegResizer::getOriginalDimensions(const std::string& inputPath, int& width, int& height) {
    if (avformat_open_input(&inputFormatContext, inputPath.c_str(), nullptr, nullptr) < 0) {
        return false;
    }

    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        avformat_close_input(&inputFormatContext);
        return false;
    }

    for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
        if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            width = inputFormatContext->streams[i]->codecpar->width;
            height = inputFormatContext->streams[i]->codecpar->height;
            avformat_close_input(&inputFormatContext);
            return true;
        }
    }

    avformat_close_input(&inputFormatContext);
    return false;
}

int FFmpegResizer::calculateHeight(int targetWidth, int originalWidth, int originalHeight) {
    return static_cast<int>(round(static_cast<double>(originalHeight) * targetWidth / originalWidth));
}

void FFmpegResizer::resizeWithPreset(const std::string& inputPath, const std::string& outputPath, ImageSize size) {
    int originalWidth, originalHeight;
    if (!getOriginalDimensions(inputPath, originalWidth, originalHeight)) {
        throw std::runtime_error("Could not get original image dimensions");
    }

    int targetWidth;
    switch (size) {
        case ImageSize::SMALL:
            targetWidth = SMALL_WIDTH;
            break;
        case ImageSize::MEDIUM:
            targetWidth = MEDIUM_WIDTH;
            break;
        case ImageSize::LARGE:
            targetWidth = LARGE_WIDTH;
            break;
        default:
            throw std::runtime_error("Invalid preset size");
    }

    int targetHeight = calculateHeight(targetWidth, originalWidth, originalHeight);
    resize(inputPath, outputPath, targetWidth, targetHeight);
}

void FFmpegResizer::resize(const std::string& inputPath, const std::string& outputPath, int dstWidth, int dstHeight) {
    // Open input file and prepare input format context
    if (avformat_open_input(&inputFormatContext, inputPath.c_str(), nullptr, nullptr) != 0) {
        throw std::runtime_error("Error opening input file: " + inputPath);
    }

    // Find stream info
    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        throw std::runtime_error("Error finding stream info for input file: " + inputPath);
    }

    // Find the video stream index
    for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
        if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        throw std::runtime_error("Could not find video stream in input file: " + inputPath);
    }

    // Get codec context for the video stream
    AVCodecParameters* codecParams = inputFormatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecParams->codec_id);
    if (!decoder) {
        throw std::runtime_error("Error finding decoder for the video stream");
    }

    codecContext = avcodec_alloc_context3(decoder);
    if (avcodec_parameters_to_context(codecContext, codecParams) < 0) {
        throw std::runtime_error("Error copying codec parameters to codec context");
    }

    if (avcodec_open2(codecContext, decoder, nullptr) < 0) {
        throw std::runtime_error("Error opening codec");
    }

    // Allocate frame and packet
    frame = av_frame_alloc();
    packet = av_packet_alloc();
    if (!frame || !packet) {
        throw std::runtime_error("Could not allocate frame or packet");
    }

    // Create scaling context
    swsContext = sws_getContext(
        codecContext->width, codecContext->height, codecContext->pix_fmt,
        dstWidth, dstHeight, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsContext) {
        throw std::runtime_error("Could not initialize scaling context");
    }

    // Allocate destination image buffer
    int ret = av_image_alloc(dstData, dstLinesize, dstWidth, dstHeight, AV_PIX_FMT_RGB24, 1);
    if (ret < 0) {
        throw std::runtime_error("Could not allocate destination image");
    }

    // Read frames
    while (av_read_frame(inputFormatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            if (processPacket(dstWidth, dstHeight)) {
                // Write output file (first frame only)
                writeJPEG(outputPath, dstWidth, dstHeight);
                break;
            }
        }
        av_packet_unref(packet);
    }

    cleanup();
}

bool FFmpegResizer::processPacket(int dstWidth, int dstHeight) {
    int ret = avcodec_send_packet(codecContext, packet);
    if (ret < 0) {
        return false;
    }

    ret = avcodec_receive_frame(codecContext, frame);
    if (ret < 0) {
        return false;
    }

    // Scale the image
    sws_scale(swsContext,
             frame->data, frame->linesize, 0, codecContext->height,
             dstData, dstLinesize);
    return true;
}

void FFmpegResizer::writeJPEG(const std::string& outputPath, int dstWidth, int dstHeight) {
    const AVCodec* jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec) {
        throw std::runtime_error("Could not find JPEG encoder");
    }

    AVCodecContext* jpegContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegContext) {
        throw std::runtime_error("Could not allocate JPEG context");
    }

    jpegContext->width = dstWidth;
    jpegContext->height = dstHeight;
    jpegContext->time_base = AVRational{1, 25};
    jpegContext->pix_fmt = AV_PIX_FMT_YUVJ420P;
    jpegContext->codec_type = AVMEDIA_TYPE_VIDEO;

    if (avcodec_open2(jpegContext, jpegCodec, nullptr) < 0) {
        avcodec_free_context(&jpegContext);
        throw std::runtime_error("Could not open JPEG encoder");
    }

    AVFrame* jpegFrame = av_frame_alloc();
    jpegFrame->width = dstWidth;
    jpegFrame->height = dstHeight;
    jpegFrame->format = AV_PIX_FMT_YUVJ420P;
    
    if (av_frame_get_buffer(jpegFrame, 0) < 0) {
        av_frame_free(&jpegFrame);
        avcodec_free_context(&jpegContext);
        throw std::runtime_error("Could not allocate JPEG frame buffer");
    }

    SwsContext* rgbToYuvContext = sws_getContext(
        dstWidth, dstHeight, AV_PIX_FMT_RGB24,
        dstWidth, dstHeight, AV_PIX_FMT_YUVJ420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    sws_scale(rgbToYuvContext,
             dstData, dstLinesize, 0, dstHeight,
             jpegFrame->data, jpegFrame->linesize);

    FILE* outFile = fopen(outputPath.c_str(), "wb");
    if (!outFile) {
        sws_freeContext(rgbToYuvContext);
        av_frame_free(&jpegFrame);
        avcodec_free_context(&jpegContext);
        throw std::runtime_error("Could not open output file");
    }

    AVPacket* jpegPacket = av_packet_alloc();
    
    avcodec_send_frame(jpegContext, jpegFrame);
    avcodec_receive_packet(jpegContext, jpegPacket);
    
    fwrite(jpegPacket->data, 1, jpegPacket->size, outFile);
    
    fclose(outFile);
    av_packet_free(&jpegPacket);
    sws_freeContext(rgbToYuvContext);
    av_frame_free(&jpegFrame);
    avcodec_free_context(&jpegContext);
}

void FFmpegResizer::cleanup() {
    if (dstData[0]) {
        av_freep(&dstData[0]);
    }
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (packet) {
        av_packet_free(&packet);
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }
    if (inputFormatContext) {
        avformat_close_input(&inputFormatContext);
    }
}