/*
 * src/img2csv/main.cpp
 *
 * Step 1 (C++): Loading and Conversion
 *
 * Objective: isolate the complexity of proprietary image formats
 * and produce a grayscale matrix as CSV.
 *
 * Input:  PNG image
 * Output: CSV file, one line per pixel row, values 0-255
 *
 * Requires: stb_image.h
 *
 * Exit codes: 0 = OK, 1 = runtime error, 2 = usage error.
 *
 * Build: g++ -std=c++2b -O2 -D_GLIBCXX_ASSERTIONS -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
 *            main.cpp -o img2csv
 *        (for tests, add: -g -fsanitize=address,undefined)
 */

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// stb_image is meant for trusted data; for anything else its docs advise lowering this limit
// so absurd dimensions are rejected during header parsing, before any allocation.
#define STBI_MAX_DIMENSIONS (1 << 16)
#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

namespace {

template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::uint64_t DEFAULT_MAX_PIXELS = 16ULL * 1024 * 1024;
constexpr std::uintmax_t MAX_FILE_BYTES = 256ULL * 1024 * 1024;
static_assert(MAX_FILE_BYTES <= std::numeric_limits<int>::max(), "stb_image takes the buffer size as int");

struct Options {
    fs::path input;
    fs::path output;
    std::uint64_t max_pixels = DEFAULT_MAX_PIXELS;
    bool help = false;
};

void usage(std::FILE *out)
{
    std::println(out, "Usage: img2csv -i <input_path> -o <output_path> [-m <max_pixels>]\n"
                 "Options:\n"
                 "  -h, --help                Print this help message and exit\n"
                 "  -i, --input <path>        Path to the input PNG file (required)\n"
                 "  -o, --output <path>       Path to the output CSV file (required)\n"
                 "  -m, --max-pixels <n>      Reject images with more than <n> pixels (default: {})",
                 DEFAULT_MAX_PIXELS);
}

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
        else {
            const char *end = value.data() + value.size();
            const auto [ptr, ec] = std::from_chars(value.data(), end, opts.max_pixels);
            if (ec != std::errc{} || ptr != end || opts.max_pixels == 0) {
                return std::unexpected(std::format("invalid value '{}' for option '{}'", value, arg));
            }
        }
    }

    if (opts.input.empty()) {
        return std::unexpected("option '-i,--input' is required");
    }
    if (opts.output.empty()) {
        return std::unexpected("option '-o,--output' is required");
    }
    if (!opts.output.has_filename()) {
        return std::unexpected(std::format("output path '{}' has no file name", opts.output.string()));
    }
    return opts;
}

using StbiPtr = std::unique_ptr < stbi_uc, decltype([](void *p)
{
    stbi_image_free(p);
}) >;

// Owns the stb buffer and only exposes it as bounded spans (no raw pointer arithmetic outside).
struct GrayImage {
    StbiPtr data;
    std::size_t width = 0;
    std::size_t height = 0;
    int source_channels = 0;

    [[nodiscard]] auto row(std::size_t y) const -> std::span<const stbi_uc>
    {
        return std::span<const stbi_uc>(data.get(), width * height).subspan(y * width, width);
    }
};

auto stb_error() -> std::string
{
    const char *reason = stbi_failure_reason();
    return reason != nullptr ? reason : "unknown error";
}

// Reads the whole file once, so header check and decode see the same bytes (no TOCTOU, Time-of-Check to Time-of-Use).
auto read_file(const fs::path &path) -> Result<std::vector<stbi_uc>>
{
    std::error_code ec;
    const auto size = fs::file_size(path, ec); // also fails for directories
    if (ec) {
        return std::unexpected(std::format("cannot stat '{}': {}", path.string(), ec.message()));
    }
    if (size == 0 || size > MAX_FILE_BYTES) {
        return std::unexpected(std::format("invalid file size: {} bytes (max {})", size, MAX_FILE_BYTES));
    }

    std::vector<stbi_uc> bytes(size);
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size))) {
        return std::unexpected(std::format("cannot read '{}'", path.string()));
    }
    return bytes;
}

auto load_gray(const fs::path &path, std::uint64_t max_pixels) -> Result<GrayImage>
{
    const auto bytes = read_file(path);
    if (!bytes) {
        return std::unexpected(bytes.error());
    }
    const int len = static_cast<int>(bytes->size()); // <= MAX_FILE_BYTES (static_assert above)
    int w = 0, h = 0, channels = 0;

    // Reads only the header: rejects huge images before allocating memory.
    if (!stbi_info_from_memory(bytes->data(), len, &w, &h, &channels)) {
        return std::unexpected(std::format("cannot read image header: {}", stb_error()));
    }
    const auto pixel_count = static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h);
    if (pixel_count > max_pixels) {
        return std::unexpected(std::format("image too large: {}x{} ({} pixels, limit {})",
                                           w, h, pixel_count, max_pixels));
    }

    GrayImage img{StbiPtr(stbi_load_from_memory(bytes->data(), len, &w, &h, &channels, STBI_grey))};
    if (!img.data) {
        return std::unexpected(stb_error());
    }
    img.width = static_cast<std::size_t>(w);
    img.height = static_cast<std::size_t>(h);
    img.source_channels = channels;
    return img;
}

// Writes to "<path>.tmp" and renames on success, so a failure never leaves a partial CSV.
auto write_csv(const fs::path &path, const GrayImage &img) -> Result<void>
{
    fs::path tmp = path;
    tmp += ".tmp";
    std::error_code ec;

    const auto fail = [&](std::string message) {
        fs::remove(tmp, ec);
        return std::unexpected(std::move(message));
    };

    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            return fail(std::format("cannot create directory '{}': {}", path.parent_path().string(), ec.message()));
        }
    }

    std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
    if (!file) {
        return fail(std::format("cannot open '{}' for writing", tmp.string()));
    }

    std::vector<char> line(img.width * 4); // up to "255," per pixel
    for (std::size_t y = 0; y < img.height; ++y) {
        char *out = line.data();
        for (const stbi_uc v : img.row(y)) {
            out = std::to_chars(out, out + 3, v).ptr;
            *out++ = ',';
        }
        out[-1] = '\n'; // replaces the trailing comma
        file.write(line.data(), out - line.data());
    }

    file.close();
    if (!file) {
        return fail("failed while writing CSV (disk full?)");
    }

    fs::rename(tmp, path, ec);
    if (ec) {
        return fail(std::format("cannot move result to '{}': {}", path.string(), ec.message()));
    }
    return {};
}

} // namespace

auto main(int argc, char **argv) -> int
{
    const std::span<const char *const> args(argv, static_cast<std::size_t>(argc));
    const auto opts = parse_args(args.empty() ? args : args.subspan(1));
    if (!opts) {
        std::println(stderr, "Error: {}", opts.error());
        usage(stderr);
        return 2;
    }
    if (opts->help) {
        usage(stdout);
        return 0;
    }

    const auto image = load_gray(opts->input, opts->max_pixels);
    if (!image) {
        std::println(stderr, "Error loading image '{}': {}", opts->input.filename().string(), image.error());
        return 1;
    }
    std::println("Image loaded: {}\nWidth: {}\nHeight: {}\nChannels: {}",
                 opts->input.string(), image->width, image->height, image->source_channels);

    // Missing ".csv" extension is appended.
    fs::path output = opts->output;
    std::string ext = output.extension().string();
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (ext != ".csv") {
        output += ".csv";
    }

    if (const auto written = write_csv(output, *image); !written) {
        std::println(stderr, "Error saving grayscale matrix to CSV: {}", written.error());
        return 1;
    }

    std::println("CSV successfully generated at: {}", output.string());
    return 0;
}
