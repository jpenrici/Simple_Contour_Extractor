// Etapa 1 (C++): Carregamento e Conversão
//
// Responsabilidade: isolar a complexidade de formatos de imagem fechados
// (BMP/PNG) e produzir uma matriz em escala de cinza como CSV.
//
// Entrada:  caminho da imagem (BMP/PNG)
// Saída:    arquivo CSV, uma linha por linha de pixel, valores 0-255
//
// Biblioteca de leitura: stb_image.h

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <span>
#include <string_view>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {

inline void usage(std::string_view program_name)
{
    std::println("Usage: {} -i <input_path> [-o <output_path>]\n"
                 "Options:\n"
                 "  -h, --help          Print this help message and exit\n"
                 "  -i, --input <path>  Path to the input file (required)\n"
                 "  -o, --output <path> Path to the output file (optional)\n", program_name);
}

inline auto to_upper(std::string str) -> std::string
{
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

}

auto main(int argc, char **argv) -> int
{
    // args
    std::span<char *> args(argv, argc);
    std::string program_name{std::filesystem::path(args[0]).filename()};

    std::string input_path;

    static constexpr std::string OUTPUT_DIR = "data/output/";
    std::string output_path = OUTPUT_DIR + "/step1.csv";

    for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];

        if (arg == "-h" || arg == "--help") {
            usage(program_name);
            return 0;
        }
        else if ((arg == "-i" || arg == "--input") && i + 1 < args.size()) {
            input_path = args[++i];
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < args.size()) {
            output_path = args[++i];
        }
    }

    if (input_path.empty()) {
        std::println("Error: Option '-i,--input' is required!");
        usage(program_name);
        return 1;
    }

    if (!std::filesystem::exists(input_path)) {
        std::println("Input path not found!");
        return 1;
    }

    auto output_dir = std::filesystem::path(output_path).parent_path();
    if (!std::filesystem::exists(output_dir)) {
        std::println("Directory not found, create: '{}'", output_dir.string());
        try {
            std::filesystem::create_directories(output_dir);
        }
        catch (std::exception e) {
            std::println("Error creating directory '{}'\nError: {}", output_dir.string(), e.what());
            return 1;
        }
    }

    // stb
    int w = 0, h = 0, channels = 0;
    unsigned char *img_data = stbi_load(input_path.c_str(), &w, &h, &channels, 0);
    if (img_data == nullptr) {
        std::println("Error loading image '{}'", std::filesystem::path(input_path).filename().string());
        return 1;
    }

    std::println("Image loaded: {}\n" "Width: {}\nHeight: {}\n" "Channels: {}", input_path, w, h, channels);

    // TODO: validar se a imagem é "simples o suficiente" para seguir no
    //       pipeline (critério a definir: resolução máxima, nº de cores
    //       distintas, etc.) — abortar com código de saída != 0 se não for.

    // CSV - Gray Matrix
    try {
        output_path = to_upper(output_path).ends_with(".CSV") ? output_path : output_path + ".csv";
        std::ofstream csv_file(output_path);
        if (!csv_file.is_open()) {
            std::println("Error saving grayscale image matrix to CSV.");
            stbi_image_free(img_data);
            return 1;
        }

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int index = (y * w + x) * channels;
                unsigned char gray_value = 0;
                if (channels == 1) {
                    gray_value = img_data[index];
                }
                else if (channels >= 3) {
                    unsigned char red = img_data[index];
                    unsigned char green = img_data[index];
                    unsigned char blue = img_data[index];
                    gray_value = static_cast<unsigned char>(0.299 * red + 0.587 * green + 0.114 * blue);
                }
                csv_file << static_cast<int>(gray_value);
                if (x < w - 1) {
                    csv_file << ",";
                }
            }
            csv_file <<  "\n";
        }

        csv_file.close();
        stbi_image_free(img_data);
    }
    catch (std::exception e) {
        std::println("Error saving grayscale image matrix to CSV.\n{}", e.what());
    }

    std::println("CSV successfully generated at: {}", output_path);

    return 0;
}
