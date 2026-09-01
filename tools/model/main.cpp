#include "le/analysis.h"
#include "le/model.h"
#include "le/reading.h"
#include "le/version.h"
#include "model/artifact.hpp"

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t baseline_model_abi = (1U << 16U) | 11U;

void usage(std::ostream& stream) {
    stream << "Usage:\n"
              "  le-model inspect FILE\n"
              "  le-model compile-prefix OUTPUT [--fixed N | --proportion P]"
              " [--language TAG] [--model-version N]\n"
              "  le-model compile-lexical-core OUTPUT [--language TAG]"
              " [--model-version N]\n"
              "  le-model compile-linear-salience OUTPUT --weight FEATURE:WEIGHT"
              " [--bias B] [--language TAG] [--model-version N]\n";
}

bool parse_u32(std::string_view text, std::uint32_t& value) {
    if (text.empty() || text.front() < '0' || text.front() > '9') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtoul(text.data(), &end, 10);
    if (errno != 0 || end != text.data() + text.size() ||
        parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_float(std::string_view text, float& value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    value = std::strtof(text.data(), &end);
    return errno == 0 && end == text.data() + text.size() && std::isfinite(value);
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open input file: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_file(const std::string& path, std::span<const std::uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not open output file: " + path);
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("could not write output file: " + path);
    }
}

const char* type_name(std::uint32_t type) {
    if (type == LE_MODEL_PREFIX) {
        return "prefix";
    }
    if (type == LE_MODEL_LEXICAL_CORE) {
        return "lexical-core";
    }
    return type == LE_MODEL_SEGMENTAL_SALIENCE ? "segmental-salience" : "linear-salience";
}

void inspect(const std::string& path) {
    const auto bytes = read_file(path);
    const auto artifact = le::model::load(bytes);
    std::cout << "{\n"
              << "  \"format\":\"" << LE_MODEL_FORMAT_VERSION_MAJOR << '.'
              << LE_MODEL_FORMAT_VERSION_MINOR << "\",\n"
              << "  \"size\":" << bytes.size() << ",\n"
              << "  \"checksum\":\"" << std::hex << std::setfill('0') << std::setw(8)
              << le::model::checksum(bytes) << std::dec << "\",\n"
              << "  \"minimum_abi\":" << artifact.minimum_abi_version << ",\n"
              << "  \"type\":\"" << type_name(artifact.type) << "\",\n"
              << "  \"model_version\":" << artifact.model_version << ",\n"
              << "  \"languages\":[";
    for (std::size_t index = 0; index < artifact.languages.size(); ++index) {
        std::cout << (index == 0 ? "" : ",") << '"' << artifact.languages[index] << '"';
    }
    std::cout << "],\n  \"required_features\":[";
    for (std::size_t index = 0; index < artifact.required_features.size(); ++index) {
        std::cout << (index == 0 ? "" : ",") << artifact.required_features[index];
    }
    std::cout << ']';
    if (artifact.type == LE_MODEL_PREFIX) {
        std::cout << ",\n  \"parameters\":{\"strategy\":\""
                  << (artifact.prefix_strategy == LE_PREFIX_FIXED ? "fixed" : "proportional")
                  << "\",\"fixed_graphemes\":" << artifact.fixed_graphemes
                  << ",\"proportion\":" << artifact.prefix_proportion << '}';
    } else if (artifact.type == LE_MODEL_LINEAR_SALIENCE ||
               artifact.type == LE_MODEL_SEGMENTAL_SALIENCE) {
        std::cout << ",\n  \"parameters\":{\"bias\":" << artifact.linear_bias << ",\"weights\":[";
        for (std::size_t index = 0; index < artifact.linear_weights.size(); ++index) {
            const auto& item = artifact.linear_weights[index];
            std::cout << (index == 0 ? "" : ",") << "{\"feature\":" << item.feature
                      << ",\"weight\":" << item.weight << '}';
        }
        std::cout << ']';
        if (artifact.type == LE_MODEL_SEGMENTAL_SALIENCE) {
            std::cout << ",\"anchor_bias\":" << artifact.segmental_anchor_bias
                      << ",\"anchor_weights\":[";
            for (std::size_t index = 0; index < artifact.segmental_anchor_weights.size(); ++index) {
                const auto& item = artifact.segmental_anchor_weights[index];
                std::cout << (index == 0 ? "" : ",") << "{\"feature\":" << item.feature
                          << ",\"weight\":" << item.weight << '}';
            }
            std::cout << "],\"minimum_graphemes\":" << artifact.segmental_minimum_graphemes;
        }
        std::cout << '}';
    }
    std::cout << "\n}\n";
}

void compile(int argc, char** argv, bool lexical_core) {
    if (argc < 3) {
        usage(std::cerr);
        std::exit(2);
    }
    le::model::Artifact artifact{
        baseline_model_abi,
        lexical_core ? LE_MODEL_LEXICAL_CORE : LE_MODEL_PREFIX,
        1,
        {},
        lexical_core ? std::vector<std::uint32_t>{LE_FEATURE_LEXICAL_CORE}
                     : std::vector<std::uint32_t>{},
        LE_PREFIX_PROPORTIONAL,
        1,
        0.5F,
        0.0F,
        {},
        0.5F,
        {},
        1,
    };
    for (int index = 3; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (option == "--language" && index + 1 < argc) {
            artifact.languages.emplace_back(argv[++index]);
        } else if (option == "--model-version" && index + 1 < argc) {
            if (!parse_u32(argv[++index], artifact.model_version)) {
                throw std::runtime_error("invalid model version");
            }
        } else if (!lexical_core && option == "--fixed" && index + 1 < argc) {
            artifact.prefix_strategy = LE_PREFIX_FIXED;
            if (!parse_u32(argv[++index], artifact.fixed_graphemes)) {
                throw std::runtime_error("invalid fixed grapheme count");
            }
        } else if (!lexical_core && option == "--proportion" && index + 1 < argc) {
            artifact.prefix_strategy = LE_PREFIX_PROPORTIONAL;
            if (!parse_float(argv[++index], artifact.prefix_proportion)) {
                throw std::runtime_error("invalid prefix proportion");
            }
        } else {
            throw std::runtime_error("unrecognized or incomplete model option");
        }
    }
    const auto bytes = le::model::encode(artifact);
    write_file(argv[2], bytes);
}

void compile_linear_salience(int argc, char** argv) {
    if (argc < 3) {
        usage(std::cerr);
        std::exit(2);
    }
    le::model::Artifact artifact{LE_ABI_VERSION,
                                 LE_MODEL_LINEAR_SALIENCE,
                                 1,
                                 {},
                                 {},
                                 LE_PREFIX_PROPORTIONAL,
                                 1,
                                 0.5F,
                                 0.0F,
                                 {},
                                 0.5F,
                                 {},
                                 1};
    for (int index = 3; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (option == "--language" && index + 1 < argc) {
            artifact.languages.emplace_back(argv[++index]);
        } else if (option == "--model-version" && index + 1 < argc) {
            if (!parse_u32(argv[++index], artifact.model_version)) {
                throw std::runtime_error("invalid model version");
            }
        } else if (option == "--bias" && index + 1 < argc) {
            if (!parse_float(argv[++index], artifact.linear_bias)) {
                throw std::runtime_error("invalid linear salience bias");
            }
        } else if (option == "--weight" && index + 1 < argc) {
            const std::string_view specification(argv[++index]);
            const auto separator = specification.find(':');
            std::uint32_t feature = 0;
            float weight = 0.0F;
            if (separator == std::string_view::npos || separator == 0 ||
                separator + 1 == specification.size() ||
                !parse_u32(specification.substr(0, separator), feature) ||
                !parse_float(specification.substr(separator + 1), weight)) {
                throw std::runtime_error("invalid feature:weight specification");
            }
            artifact.required_features.push_back(feature);
            artifact.linear_weights.push_back(le::model::Artifact::FeatureWeight{feature, weight});
        } else {
            throw std::runtime_error("unrecognized or incomplete model option");
        }
    }
    write_file(argv[2], le::model::encode(artifact));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(std::cerr);
        return 2;
    }
    try {
        const std::string_view command(argv[1]);
        if (command == "inspect" && argc == 3) {
            inspect(argv[2]);
        } else if (command == "compile-prefix") {
            compile(argc, argv, false);
        } else if (command == "compile-lexical-core") {
            compile(argc, argv, true);
        } else if (command == "compile-linear-salience") {
            compile_linear_salience(argc, argv);
        } else {
            usage(std::cerr);
            return 2;
        }
        return 0;
    } catch (const le::model::ArtifactError& error) {
        std::cerr << "Model artifact error: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Model tool error: " << error.what() << '\n';
        return 1;
    }
}
