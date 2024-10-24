#ifndef FFMPEG_RESIZER_HPP
#define FFMPEG_RESIZER_HPP

#include <iostream>
#include <stdexcept>
#include <memory>
#include <cmath>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

// Preset sizes
const int SMALL_WIDTH = 250;
const int MEDIUM_WIDTH = 350;
const int LARGE_WIDTH = 650;

enum class ImageSize {
    SMALL,
    MEDIUM,
    LARGE,
    CUSTOM
};

class FFmpegResizer {
private:
    AVFormatContext* inputFormatContext = nullptr;
    AVCodecContext* codecContext = nullptr;
    SwsContext* swsContext = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    uint8_t* dstData[4] = {nullptr};
    int dstLinesize[4] = {0};
    int videoStreamIndex = -1;
    int originalWidth = 0;
    int originalHeight = 0;

public:
    ~FFmpegResizer();

    bool getOriginalDimensions(const std::string& inputPath, int& width, int& height);
    int calculateHeight(int targetWidth, int originalWidth, int originalHeight);
    void resizeWithPreset(const std::string& inputPath, const std::string& outputPath, ImageSize size);
    void resize(const std::string& inputPath, const std::string& outputPath, int dstWidth, int dstHeight);
    
private:
    bool processPacket(int dstWidth, int dstHeight);
    void writeJPEG(const std::string& outputPath, int dstWidth, int dstHeight);
    void cleanup();
};

#endif // FFMPEG_RESIZER_H
