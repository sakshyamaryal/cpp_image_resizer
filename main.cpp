#include <iostream>
#include <unistd.h>
#include "ImageResizer.h"

int main() {
    ImageResizer resizer;

    std::string inputPath = "/Users/mac/Desktop/cpp/input.jpg";
    std::string outputPath = "input.jpg";
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        std::string fullOutputPath = std::string(cwd) + "/" + outputPath;
        std::cout << "Full output path: " << fullOutputPath << std::endl;
    } else {
        std::cerr << "Failed to get current working directory." << std::endl;
    }

    if (resizer.resizeImage(inputPath, outputPath, ImageResizer::Size::SMALL)) {
        std::cout << "Image resized successfully to small size." << std::endl;
        std::cout << "Output path: " << outputPath << std::endl;
    } else {
        std::cerr << "Failed to resize image." << std::endl;
    }

    return 0;
} 