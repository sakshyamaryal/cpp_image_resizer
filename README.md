# Image Resizer

A command-line tool for resizing images using FFmpeg libraries.

## Table of Contents

* [Prerequisites](#prerequisites)
* [Installation](#installation)
* [Usage](#usage)
* [Example](#example)

## Prerequisites

* FFmpeg: A multimedia framework for handling video, audio, and other multimedia files and streams.
* macOS (for installation instructions)

## Installation

#  Install FFmpeg via Homebrew:

* brew install ffmpeg
* brew install pkg-config

#  Compile the program:

* g++ -std=c++11 task1.cpp -o resize_image `pkg-config --cflags --libs libavformat libavcodec libswscale libavutil` -lavcodec -lavformat -lavutil -lswscale

# Usage
#  Run the program:
* ./resize_image input_filepath output_filepath -- dummy example
* ./resize_image input.jpg output.jpg

# Enter the desired size (small, medium, large):
* Enter desired size (small, medium, large): small

# complete example of running the program:
   (base) mac@Sakshyam cpp % ./resize_image input.jpg output.jpg
    ''' Enter desired size (small, medium, large): small
    [swscaler @ 0x118008000] deprecated pixel format used, make sure you did set range correctly
    [swscaler @ 0x108018000] deprecated pixel format used, make sure you did set range correctly
    Created small version
    Resized version created successfully ''''