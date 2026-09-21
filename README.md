# Simple Contour Extractor

A tool that extracts edges from a simple image and converts them to SVG.

Pipeline:

    Imagem PNG
      -> (C++, stb_image)      grayscale matrix -> CSV
      -> (Fortran, Canny)      ordered contours -> JSON (custom schema)
      -> (C++, RDP + path "L") Final SVG (without Bézier curves)

## Requirements

- GCC 16 (fornece `g++` e `gfortran`)
- CMake >= 3.28
- Python 3.13+
- stb_image.h (https://github.com/nothings/stb/blob/master/stb_image.h)

## Build

    mkdir build && cd build
    cmake ..
    cmake --build .

## Run the pipeline

    python3 run.py --input path/to/image.png [--epsilon N] [--name NAME] [--no-overlay] # real image (external)
    python3 run.py [--width N] [--height N] [--noise N] [--seed N] [--epsilon N] # test image
    python3 run.py --help # to observe details
    
    Example:
    
        python3 run.py --input images/sample.png --name Sample  # processes the pipeline for the image sample.png
        python3 run.py # generates a test image and processes the pipeline
