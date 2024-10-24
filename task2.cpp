/** 
 * program that converts any video file into mp4 format using libav library and extract thumbnail.
 * Use the Library you’ve created from TASK 1 to resize the the thumbnail into 3 different sizes
 * Expectation:
 * $ ./converter video.webm should generate:
 * converted_video.mp4, thumbail_small.jpg, thumbnail_medium.jpg, thumbnail_large.jpg
*/

#include <iostream>
#include <stdexcept>
#include <memory>
#include <cmath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include "FFmpegResizer.hpp"

class VideoConverter {
public:
    void convertToMP4(const std::string& inputPath, const std::string& outputPath) {
        AVFormatContext* inputFormatContext = nullptr;
        AVFormatContext* outputFormatContext = nullptr;

        // Open input file
        if (avformat_open_input(&inputFormatContext, inputPath.c_str(), nullptr, nullptr) < 0) {
            throw std::runtime_error("Could not open input file");
        }

        // Create output format context
        avformat_alloc_output_context2(&outputFormatContext, nullptr, "mp4", outputPath.c_str());
        if (!outputFormatContext) {
            avformat_close_input(&inputFormatContext);
            throw std::runtime_error("Could not create output context");
        }

        // Find video stream and add it to output context
        for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
            if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                AVStream* outStream = avformat_new_stream(outputFormatContext, nullptr);
                if (!outStream) {
                    avformat_close_input(&inputFormatContext);
                    avformat_free_context(outputFormatContext);
                    throw std::runtime_error("Could not allocate stream");
                }
                
                // Copy codec parameters from input to output
                avcodec_parameters_copy(outStream->codecpar, inputFormatContext->streams[i]->codecpar);
                outStream->codecpar->codec_tag = 0; // Set codec tag to zero for MP4
            }
        }

        // Open output file
        if (avio_open(&outputFormatContext->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            throw std::runtime_error("Could not open output file");
        }

        // Write the output file header
        if (avformat_write_header(outputFormatContext, nullptr) < 0) {
            avformat_close_input(&inputFormatContext);
            avio_closep(&outputFormatContext->pb);
            avformat_free_context(outputFormatContext);
            throw std::runtime_error("Could not write output header");
        }

        // Read packets from input and write them to output
        AVPacket packet;
        while (av_read_frame(inputFormatContext, &packet) >= 0) {
            av_interleaved_write_frame(outputFormatContext, &packet);
            av_packet_unref(&packet);
        }

        // Write the trailer
        av_write_trailer(outputFormatContext);
        avformat_close_input(&inputFormatContext);
        avio_closep(&outputFormatContext->pb);
        avformat_free_context(outputFormatContext);
    }

    void extractThumbnail(const std::string& inputPath, const std::string& thumbnailPath) {
        AVFormatContext* formatContext = nullptr;
        AVCodecContext* codecContext = nullptr;
        AVFrame* frame = nullptr;
        AVPacket* packet = nullptr;
        SwsContext* swsContext = nullptr;

        try {
            // Open the input file
            if (avformat_open_input(&formatContext, inputPath.c_str(), nullptr, nullptr) < 0) {
                throw std::runtime_error("Could not open input file");
            }

            // Retrieve stream information
            if (avformat_find_stream_info(formatContext, nullptr) < 0) {
                throw std::runtime_error("Could not find stream information");
            }

            // Find the first video stream
            int videoStreamIndex = -1;
            for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
                if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                    videoStreamIndex = i;
                    break;
                }
            }

            if (videoStreamIndex == -1) {
                throw std::runtime_error("Could not find video stream");
            }

            // Get a pointer to the codec context for the video stream
            const AVCodec* codec = avcodec_find_decoder(formatContext->streams[videoStreamIndex]->codecpar->codec_id);
            if (!codec) {
                throw std::runtime_error("Unsupported codec");
            }

            codecContext = avcodec_alloc_context3(codec);
            if (!codecContext) {
                throw std::runtime_error("Failed to allocate codec context");
            }

            if (avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex]->codecpar) < 0) {
                throw std::runtime_error("Failed to copy codec parameters to codec context");
            }

            if (avcodec_open2(codecContext, codec, nullptr) < 0) {
                throw std::runtime_error("Failed to open codec");
            }

            frame = av_frame_alloc();
            packet = av_packet_alloc();

            if (!frame || !packet) {
                throw std::runtime_error("Failed to allocate frame or packet");
            }

            // Seek to 10% of the video duration
            int64_t duration = formatContext->duration;
            int64_t seekTarget = duration / 10;
            av_seek_frame(formatContext, -1, seekTarget, AVSEEK_FLAG_BACKWARD);

            // Read frames until we get a video frame
            while (av_read_frame(formatContext, packet) >= 0) {
                if (packet->stream_index == videoStreamIndex) {
                    int response = avcodec_send_packet(codecContext, packet);
                    if (response < 0) {
                        av_packet_unref(packet);
                        continue;
                    }

                    response = avcodec_receive_frame(codecContext, frame);
                    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                        av_packet_unref(packet);
                        continue;
                    } else if (response < 0) {
                        throw std::runtime_error("Error while decoding");
                    }

                    // We have a valid video frame, let's save it as a thumbnail
                    swsContext = sws_getContext(
                        codecContext->width, codecContext->height, codecContext->pix_fmt,
                        codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr
                    );

                    if (!swsContext) {
                        throw std::runtime_error("Could not initialize scaling context");
                    }

                    // Allocate RGB frame
                    AVFrame* rgbFrame = av_frame_alloc();
                    rgbFrame->format = AV_PIX_FMT_RGB24;
                    rgbFrame->width = codecContext->width;
                    rgbFrame->height = codecContext->height;
                    av_frame_get_buffer(rgbFrame, 0);

                    // Convert frame to RGB
                    sws_scale(swsContext, frame->data, frame->linesize, 0, codecContext->height,
                              rgbFrame->data, rgbFrame->linesize);

                    // Save the RGB frame as JPEG
                    saveFrameAsJPEG(rgbFrame, thumbnailPath);

                    // Cleanup
                    av_frame_free(&rgbFrame);
                    sws_freeContext(swsContext);

                    // Use FFmpegResizer to create different sizes
                    FFmpegResizer resizer;
                    resizer.resizeWithPreset(thumbnailPath, thumbnailPath + "_small.jpg", ImageSize::SMALL);
                    resizer.resizeWithPreset(thumbnailPath, thumbnailPath + "_medium.jpg", ImageSize::MEDIUM);
                    resizer.resizeWithPreset(thumbnailPath, thumbnailPath + "_large.jpg", ImageSize::LARGE);

                    break;
                }
                av_packet_unref(packet);
            }

        } catch (const std::exception& e) {
            // Cleanup
            if (codecContext) avcodec_free_context(&codecContext);
            if (formatContext) avformat_close_input(&formatContext);
            if (frame) av_frame_free(&frame);
            if (packet) av_packet_free(&packet);
            throw;
        }

        // Cleanup
        if (codecContext) avcodec_free_context(&codecContext);
        if (formatContext) avformat_close_input(&formatContext);
        if (frame) av_frame_free(&frame);
        if (packet) av_packet_free(&packet);
    }

private:
    void saveFrameAsJPEG(AVFrame* frame, const std::string& filename) {
        const AVCodec* jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
        if (!jpegCodec) {
            throw std::runtime_error("JPEG codec not found");
        }

        AVCodecContext* jpegContext = avcodec_alloc_context3(jpegCodec);
        if (!jpegContext) {
            throw std::runtime_error("Could not allocate JPEG codec context");
        }

        jpegContext->width = frame->width;
        jpegContext->height = frame->height;
        jpegContext->time_base = (AVRational){1, 25};
        jpegContext->pix_fmt = AV_PIX_FMT_YUVJ420P;

        if (avcodec_open2(jpegContext, jpegCodec, nullptr) < 0) {
            avcodec_free_context(&jpegContext);
            throw std::runtime_error("Could not open JPEG codec");
        }

        AVFrame* yuvFrame = av_frame_alloc();
        yuvFrame->format = AV_PIX_FMT_YUVJ420P;
        yuvFrame->width = frame->width;
        yuvFrame->height = frame->height;
        av_frame_get_buffer(yuvFrame, 0);

        SwsContext* rgbToYuvContext = sws_getContext(
            frame->width, frame->height, AV_PIX_FMT_RGB24,
            frame->width, frame->height, AV_PIX_FMT_YUVJ420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        sws_scale(rgbToYuvContext, frame->data, frame->linesize, 0, frame->height,
                  yuvFrame->data, yuvFrame->linesize);

        AVPacket* pkt = av_packet_alloc();
        int ret = avcodec_send_frame(jpegContext, yuvFrame);
        if (ret < 0) {
            av_packet_free(&pkt);
            av_frame_free(&yuvFrame);
            avcodec_free_context(&jpegContext);
            sws_freeContext(rgbToYuvContext);
            throw std::runtime_error("Error sending frame to JPEG encoder");
        }

        ret = avcodec_receive_packet(jpegContext, pkt);
        if (ret < 0) {
            av_packet_free(&pkt);
            av_frame_free(&yuvFrame);
            avcodec_free_context(&jpegContext);
            sws_freeContext(rgbToYuvContext);
            throw std::runtime_error("Error receiving packet from JPEG encoder");
        }

        FILE* f = fopen(filename.c_str(), "wb");
        if (!f) {
            av_packet_free(&pkt);
            av_frame_free(&yuvFrame);
            avcodec_free_context(&jpegContext);
            sws_freeContext(rgbToYuvContext);
            throw std::runtime_error("Could not open output file");
        }

        fwrite(pkt->data, 1, pkt->size, f);
        fclose(f);

        av_packet_free(&pkt);
        av_frame_free(&yuvFrame);
        avcodec_free_context(&jpegContext);
        sws_freeContext(rgbToYuvContext);
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_video_file>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = "converted_video.mp4";
    std::string thumbnailPath = "thumbnail.jpg"; // Temporary thumbnail file

    try {
        VideoConverter converter;

        // Convert video to MP4 format
        converter.convertToMP4(inputPath, outputPath);
        std::cout << "Converted video to " << outputPath << std::endl;

        // Extract a thumbnail from the video (this can be a frame from the video)
        converter.extractThumbnail(inputPath, thumbnailPath);
        std::cout << "Thumbnail created and resized successfully." << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}