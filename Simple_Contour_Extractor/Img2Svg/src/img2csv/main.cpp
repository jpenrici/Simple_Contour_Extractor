/*
 * src/img2csv/img2csv.cpp
 *
 * Step 1 (C++): Loading and Conversion
 *
 * Responsibility: isolate the complexity of proprietary image formats
 * and produce a grayscale matrix as CSV.
 *
 * Input:  image path (PNG)
 * Output: CSV file, one line per pixel row, values ​​0-255
 *
 * Reading library: stb_image.h
 *
 * Exit codes: 0 = OK, 1 = runtime error, 2 = usage error.
*/

#include <algorithm>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <vector>

#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {

template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::string_view DEFAULT_OUTPUT_DIR = "output";
constexpr std::string_view DEFAULT_OUTPUT_FILE = "step1.csv";
constexpr std::uint64_t DEFAULT_MAX_PIXELS = 16ULL * 1024 * 1024;

struct Options {
    std::filesystem::path input;
    std::filesystem::path output;
    std::uint64_t max_pixels = DEFAULT_MAX_PIXELS;
    bool help = false;
};

void usage()
{
    std::string program = "img2csv";
    std::println("Usage: {} -i <input_path> [-o <output_path>] [-m <max_pixels>]\n"
                 "Options:\n"
                 "  -h, --help                Print this help message and exit\n"
                 "  -i, --input <path>        Path to the input BMP/PNG file (required)\n"
                 "  -o, --output <path>       Path to the output CSV file (default: {}/{})\n"
                 "  -m, --max-pixels <n>      Reject images with more than <n> pixels (default: {})\n",
                 program, DEFAULT_OUTPUT_DIR, DEFAULT_OUTPUT_FILE, DEFAULT_MAX_PIXELS);
}

auto parse_args(std::span<const char *const> args) -> Result<Options>
{
    Options opts;

    auto parse_uint64 = [](std::string_view text) -> std::optional<std::uint64_t> {
        std::uint64_t value = 0;
        const char *end = text.data() + text.size();
        const auto [ptr, ec] = std::from_chars(text.data(), end, value);
        if (ec != std::errc{} || ptr != end || value == 0)
        {
            return std::nullopt;
        }
        return value;
    };

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];

        if (arg == "-h" || arg == "--help") {
            opts.help = true;
            return opts;
        }

        const bool is_input = arg == "-i" || arg == "--input";
        const bool is_output = arg == "-o" || arg == "--output";
        const bool is_max = arg == "-m" || arg == "--max-pixels";

        if (!is_input && !is_output && !is_max) {
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
        else if (const auto n = parse_uint64(value)) {
            opts.max_pixels = *n;
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

auto find_dir(const std::filesystem::path directory) -> std::optional<std::filesystem::path>
{
    auto current = std::filesystem::current_path();
    while (true) {
        std::filesystem::path candidate = current / directory;
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
            return candidate;
        }
        if (current == current.parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return std::nullopt;
}

auto validate_extension(const std::filesystem::path &path, const std::string_view extension) -> bool
{
    const std::string ext = path.extension().string();
    return std::ranges::equal(ext, extension, [](unsigned char a, unsigned char b) {
        return std::tolower(a) == std::tolower(b);
    });
}

struct StbiDeleter {
    void operator()(stbi_uc *p) const noexcept
    {
        stbi_image_free(p);
    }
};

struct GrayImage {
    std::unique_ptr<stbi_uc, StbiDeleter> pixels;
    std::size_t width = 0;
    std::size_t height = 0;
    int source_channels = 0;

    [[nodiscard]] auto row(std::size_t y) const -> std::span<const stbi_uc>
    {
        return {pixels.get() + y * width, width};
    }
};

auto stb_error() -> std::string
{
    const char *reason = stbi_failure_reason();
    return reason != nullptr ? reason : "unknown error";
}

auto load_gray(const std::filesystem::path &path, std::uint64_t max_pixels) -> Result<GrayImage>
{
    const std::string file = path.string();
    int w = 0, h = 0, channels = 0;

    // Reads only the header: rejects huge images before allocating memory.
    if (!stbi_info(file.c_str(), &w, &h, &channels)) {
        return std::unexpected(std::format("cannot read image header: {}", stb_error()));
    }
    const auto pixel_count = static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h);
    if (w <= 0 || h <= 0 || pixel_count > max_pixels) {
        return std::unexpected(
                   std::format("image too large or invalid: {}x{} ({} pixels, limit {})",
                               w, h, pixel_count, max_pixels));
    }

    GrayImage img;
    img.pixels.reset(stbi_load(file.c_str(), &w, &h, &channels, STBI_grey));
    if (!img.pixels) {
        return std::unexpected(stb_error());
    }
    img.width = static_cast<std::size_t>(w);
    img.height = static_cast<std::size_t>(h);
    img.source_channels = channels;
    return img;
}

auto write_csv(const std::filesystem::path &path, const GrayImage &img) -> Result<void>
{
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    std::error_code ec;

    {
        std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
        if (!file) {
            return std::unexpected(std::format("cannot open '{}' for writing", tmp.string()));
        }

        std::vector<char> line(img.width * 4);
        for (std::size_t y = 0; y < img.height; ++y) {
            char *out = line.data();
            for (const stbi_uc v : img.row(y)) {
                out = std::to_chars(out, out + 3, v).ptr;
                *out++ = ',';
            }
            out[-1] = '\n';
            file.write(line.data(), out - line.data());
        }

        file.flush();
        if (!file) {
            file.close();
            std::filesystem::remove(tmp, ec);
            return std::unexpected("failed while writing CSV (disk full?)");
        }
    }

    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        const std::string message = ec.message();
        std::filesystem::remove(tmp, ec);
        return std::unexpected(std::format("cannot move result to '{}': {}", path.string(), message));
    }
    return {};
}

} // namespace

auto main(int argc, char **argv) -> int
{
    // Args
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

    // Stb
    const auto image = load_gray(opts->input, opts->max_pixels);
    if (!image) {
        std::println(stderr, "Error loading image '{}': {}", opts->input.filename().string(), image.error());
        return 1;
    }

    std::println("Image loaded: {}\nWidth: {}\nHeight: {}\nChannels: {}", opts->input.string(), image->width,
                 image->height, image->source_channels);

    // Output
    std::filesystem::path output = opts->output;
    std::filesystem::path output_dir = output.parent_path();
    std::filesystem::path output_filename = output.filename();

    if (output_dir.empty()) {
        std::println("Directory is empty, use: {}", DEFAULT_OUTPUT_DIR);
        output_dir = DEFAULT_OUTPUT_DIR;
    }

    if (!find_dir(output_dir)) {
        std::println("Directory not found: {}", output_dir.string());
        output_dir = DEFAULT_OUTPUT_DIR;
    }

    if (output_filename.empty()) {
        std::println("Filename is empty, use: {}", DEFAULT_OUTPUT_FILE);
        output_filename = DEFAULT_OUTPUT_FILE;
    }

    output = output_dir / output_filename;
    if (!validate_extension(output, ".csv")) {
        output += ".csv";
    }

    // CSV
    if (const auto written = write_csv(output, *image); !written) {
        std::println(stderr, "Error saving grayscale matrix to CSV: {}", written.error());
        return 1;
    }

    std::println("CSV successfully generated at: {}", output.string());
    return 0;
}
