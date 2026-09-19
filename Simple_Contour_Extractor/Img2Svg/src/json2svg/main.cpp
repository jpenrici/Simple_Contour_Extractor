// src/json2svg/main.cpp
//
// Etapa 3 (C++): Base para o SVG
//
// Responsabilidade: ler o JSON de contornos, simplificar cada contorno com Ramer-Douglas-Peucker
// e escrever um SVG final usando SOMENTE comandos de linha ("L") no path, sem ajuste de curvas de Bezier.
//
// Schema do JSON de contornos (formato específico do projeto)
//
// {
//     "width": 800,
//     "height": 600,
//     "edges": [
//        { "id": 1, "pontos": [ { "x": 10, "y": 15 }, { "x": 11, "y": 15 } ] }
//     ]
// }
//
// Regras:
// - `width` e `height`: inteiros, dimensões da imagem original.
// - `edges`: lista de objetos, cada um com:
// - `id`: inteiro, identificador do contorno (resultado do edge linking).
// - `points`: lista ORDENADA de `{x, y}` (inteiros), na ordem em que o
//    contorno é percorrido. A ordem importa para o RDP e para a geração do path SVG.
//
// Entrada: Json específico
// Saída: SVG de contornos
//
// Exit codes: 0 = OK, 1 = runtime error, 2 = usage error.

#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <print>
#include <string_view>
#include <vector>

namespace {

template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view DEFAULT_OUTPUT = "data/output/step3.svg";

void usage()
{
    std::string program = "json2svg";
    std::println("Usage: {} -i <input_path> [-o <output_path>]\n"
                 "Options:\n"
                 "  -h, --help           Print this help message and exit\n"
                 "  -i, --input <path>   Path to the input JSON file (required)\n"
                 "  -o, --output <path>  Path to the output SVG file (default: {})\n",
                 program, DEFAULT_OUTPUT);
}

struct Options {
    std::filesystem::path input;
    std::filesystem::path output{DEFAULT_OUTPUT};
    bool help = false;
};

auto parse_args(std::span<const char *const> args) -> Result<Options>
{
    Options opts;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];

        if (arg == "-h" || arg == "--help") {
            opts.help = true;
            return opts;
        }

        const bool is_input = arg == "-i" || arg == "--input";
        const bool is_output = arg == "-o" || arg == "--output";

        if (!is_input && !is_output) {
            return std::unexpected(std::format("unknown argument '{}'", arg));
        }
        if (++i >= args.size()) {
            return std::unexpected(std::format("option '{}' requires a value", arg));
        }

        const std::string_view value = args[i];
        if (is_input) {
            opts.input = value;
        }
        else if (is_output) {
            opts.output = value;
        }
        else {
            return std::unexpected(std::format("invalid value '{}' for option '{}'", value, arg));
        }
    }

    if (opts.input.empty()) {
        return std::unexpected("option '-i,--input' is required");
    }
    return opts;
}

struct Point { double x; double y; };
struct Contour { int id; std::vector<Point> points; };

struct JsonImage {
    int width = 0;
    int height = 0;
    std::vector<Contour> contours;
};

auto validate_extension(const std::filesystem::path &path, const std::string_view extension) -> bool
{
    const std::string ext = path.extension().string();
    return std::ranges::equal(ext, extension, [](unsigned char a, unsigned char b) {
        return std::tolower(a) == std::tolower(b);
    });
}

auto load_json(const std::filesystem::path &path) -> Result<JsonImage> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::unexpected(std::format("failed to open input json file '{}'", path.string()));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    JsonImage img;

    // TO DO: JSON loader específico

    return img;
}

double perpendicular_distance(const Point &pt, const Point &line_start, const Point &line_end)
{
    double dx = line_end.x - line_start.x;
    double dy = line_end.y - line_start.y;
    double mag = std::hypot(dx, dy);

    if (mag == 0.0) {
        return std::hypot(pt.x - line_start.x, pt.y - line_start.y);
    }

    // Distance from the point to the line defined by line_start and line_end
    return (std::abs((line_end.y - line_start.y) * pt.x - (line_end.x - line_start.x)
            * pt.y + line_end.x * line_start.y - line_end.y * line_start.x) / mag);
}

void rdp_recursive(const std::vector<Point> &points, std::size_t first,
                   std::size_t last, double epsilon, std::vector<bool> &keep)
{
    if (first + 1 >= last) {
        return;
    }

    double max_dist = 0.0;
    std::size_t index = first;

    for (std::size_t i = first + 1; i < last; ++i) {
        double dist = perpendicular_distance(points[i], points[first], points[last]);
        if (dist > max_dist) {
            max_dist = dist;
            index = i;
        }
    }

    if (max_dist > epsilon) {
        keep[index] = true;
        rdp_recursive(points, first, index, epsilon, keep);
        rdp_recursive(points, index, last, epsilon, keep);
    }
}

auto ramer_douglas_peucker(const std::vector<Point> &points, double epsilon) -> std::vector<Point>
{
    if (points.size() < 3) {
        return points;
    }

    std::vector<bool> keep(points.size(), false);
    keep[0] = true;
    keep.back() = true;

    rdp_recursive(points, 0, points.size() - 1, epsilon, keep);

    std::vector<Point> simplified;
    simplified.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (keep[i]) {
            simplified.push_back(points[i]);
        }
    }
    return simplified;
}

auto write_svg(const std::filesystem::path &path, int width, int height,
               const std::vector<Contour> &contours, double epsilon) -> Result<void>
{
    std::ofstream file(path);
    if (!file.is_open()) {
        return std::unexpected(std::format("failed to create output svg file '{}'", path.string()));
    }

    file << std::format("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{}\" height=\"{}\" viewBox=\"0 0 {} {}\">\n",
                        width, height, width, height);
    file << "  <style>\n";
    file << "    .contour { fill: none; stroke: #2c3e50; stroke-width: 1.5px; }\n";
    file << "  </style>\n";
    file << "  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";

    for (const auto &contour : contours) {
        auto simplified = ramer_douglas_peucker(contour.points, epsilon);
        if (simplified.empty()) {
            continue;
        }

        file << "  <path class=\"contour\" d=\"M " << simplified[0].x << " " << simplified[0].y;
        for (std::size_t i = 1; i < simplified.size(); ++i) {
            file << " L " << simplified[i].x << " " << simplified[i].y;
        }
        file << " Z\" />\n";
    }

    file << "</svg>\n";
    return {};
}

} // namespace

auto main(int argc, char** argv) -> int {

    const std::span<const char *const> args(argv, static_cast<std::size_t>(argc));
    const auto cli = args.empty() ? args : args.subspan(1);

    const auto opts = parse_args(cli);
    if (!opts) {
        std::println(stderr, "Error: {}\n", opts.error());
        usage();
        return 2;
    }
    if (opts->help) {
        usage();
        return 0;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(opts->input, ec)) {
        std::println(stderr, "Error: input file not found: '{}'", opts->input.string());
        return 1;
    }

    std::filesystem::path output = opts->output;
    if (!validate_extension(output, ".svg")) {
        output += ".svg";
    }

    if (const std::filesystem::path dir = output.parent_path(); !dir.empty()) {
        if (std::filesystem::create_directories(dir, ec)) {
            std::println("Directory created: '{}'", dir.string());
        }
        else if (ec) {
            std::println(stderr, "Error creating directory '{}': {}", dir.string(), ec.message());
            return 1;
        }
    }

    // TODO - Executar Sequência
    // TODO: ler json_path e popular largura, altura e vector<Contour>
    // TODO: para cada Contour, aplicar Ramer-Douglas-Peucker (epsilon
    //       configuravel) para reduzir o numero de pontos.
    // TODO: montar o path SVG usando apenas comandos M (move) e L (line),
    //       sem C/Q (Bezier) -- decisao de escopo do projeto.
    // TODO: escrever o arquivo SVG final em svg_path.

    return 0;
}
