#ifndef IMAGERESIZER_H
#define IMAGERESIZER_H

#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class ImageResizer {
public:
    enum class Size {
        SMALL = 256,
        MEDIUM = 512,
        LARGE = 1024
    };

    bool resizeImage(const std::string &inputPath, const std::string &outputPath, Size size);
};

#endif // IMAGERESIZER_H#include "ImageResizer.h"