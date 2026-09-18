// Etapa 1 (C++): Carregamento e Conversão
//
// Responsabilidade: isolar a complexidade de formatos de imagem fechados
// (BMP/PNG) e produzir uma matriz em escala de cinza como CSV.
//
// Entrada:  caminho da imagem (BMP/PNG)
// Saída:    arquivo CSV, uma linha por linha de pixel, valores 0-255
//
// Biblioteca de leitura: stb_image.h

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <print>
#include <span>
#include <string_view>
#include <filesystem>
#include <cstdlib>

auto main(int argc, char **argv) -> int
{

    auto usage = [](std::string_view program_name) {
        std::println("Usage: {0} -i <input_path> [-o <output_path>]\n"
                     "Options:\n"
                     "  -h, --help          Print this help message and exit\n"
                     "  -i, --input <path>  Path to the input file (required)\n"
                     "  -o, --output <path> Path to the output file (optional)\n", program_name);
    };

    std::span<char *> args(argv, argc);
    std::string program_name{std::filesystem::path(args[0]).filename()};

    std::filesystem::path input_path;
    std::filesystem::path output_path{"./data/output/step1.csv"};
    bool input_ok = false;

    for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];

        if (arg == "-h" || arg == "--help") {
            usage(program_name);
            return 0;
        }
        else if ((arg == "-i" || arg == "--input") && i + 1 < args.size()) {
            input_path = args[++i];
            input_ok = true;
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < args.size()) {
            output_path = args[++i];
        }
    }

    if (!input_ok || input_path.empty()) {
        std::println("Error: Option '-i,--input' is required!");
        usage(program_name);
        return 1;
    }

    // TODO: checar diretórios e formatar extensões dos arquivos (in/out);
    // TODO: copiar imagem de entrada para diretório data;
    // TODO: decodificar a imagem com stbi_load(input_path, &w, &h, &channels, 0);
    // TODO: converter para escala de cinza (0-255) em uma matriz w x h.
    // TODO: validar se a imagem é "simples o suficiente" para seguir no
    //       pipeline (critério a definir: resolução máxima, nº de cores
    //       distintas, etc.) — abortar com código de saída != 0 se não for.
    // TODO: escrever a matriz como CSV em output_path.

    std::println("img2cvs: not yet implemented ({} -> {})", input_path.string(), output_path.string());
    return 1;
}
