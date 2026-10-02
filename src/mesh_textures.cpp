#include "mesh_import.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

namespace vkz::mesh::detail {
    namespace {
        struct temporary_directory {
            path directory;

            temporary_directory() {
                static std::atomic<uint64_t> sequence{};
                for (;;) {
                    directory = std::filesystem::temp_directory_path() / ("vkz-ktx-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(sequence++));
                    if (std::filesystem::create_directory(directory)) break;
                }
            }

            ~temporary_directory() {
                std::error_code error;
                std::filesystem::remove_all(directory, error);
            }
        };

        void run_ktx(const path &input, const path &output, bool srgb) {
            const char *configured = std::getenv("VKZ_KTX_EXECUTABLE");
            const path executable = configured && *configured ? std::filesystem::u8path(configured) : path("ktx");
            const std::vector<path> arguments{executable, "create", "--format", srgb ? "R8G8B8A8_SRGB" : "R8G8B8A8_UNORM", "--assign-tf", srgb ? "srgb" : "linear", "--generate-mipmap", input, output};
            int result = -1;
#ifdef _WIN32
            // Quote the Windows argv convention directly; never invoke a shell.
            std::wstring command;
            for (const auto &argument : arguments) {
                command += L'"';
                size_t slashes = 0;
                for (wchar_t c : argument.native()) {
                    if (c == L'\\') {
                        ++slashes;
                        continue;
                    }
                    command.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
                    slashes = 0;
                    command += c;
                }
                command.append(slashes * 2, L'\\');
                command += L"\" ";
            }
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
                throw std::runtime_error("Cannot start KTX converter. Install KTX-Software or set VKZ_KTX_EXECUTABLE to ktx.");
            WaitForSingleObject(process.hProcess, INFINITE);
            DWORD status = 1;
            GetExitCodeProcess(process.hProcess, &status);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            result = int(status);
#else
            std::vector<std::string> strings;
            for (const auto &argument : arguments)
                strings.push_back(argument.string());
            std::vector<char *> argv;
            for (auto &argument : strings)
                argv.push_back(argument.data());
            argv.push_back(nullptr);
            pid_t process;
            if (posix_spawnp(&process, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
                throw std::runtime_error("Cannot start KTX converter. Install KTX-Software or set VKZ_KTX_EXECUTABLE to ktx.");
            int status;
            pid_t waited;
            do {
                waited = waitpid(process, &status, 0);
            } while (waited < 0 && errno == EINTR);
            if (waited >= 0 && WIFEXITED(status)) result = WEXITSTATUS(status);
#endif
            if (result != 0) throw std::runtime_error("KTX conversion failed with exit code " + std::to_string(result));
        }

        std::vector<uint8_t> encode(const image &source, const path &directory, bool srgb) {
            if (source.encoded_data.empty()) throw std::runtime_error("KTX conversion requires an embedded source image");
            const char *extension = source.mime_type == "image/png" ? ".png" : source.mime_type == "image/jpeg" ? ".jpg" : nullptr;
            if (!extension) throw std::runtime_error("Unsupported source image type for KTX conversion: " + source.mime_type);
            const auto input = directory / (std::string("input") + extension);
            const auto output = directory / "output.ktx2";
            {
                std::ofstream file(input, std::ios::binary);
                file.write(reinterpret_cast<const char *>(source.encoded_data.data()), source.encoded_data.size());
                if (!file) throw std::runtime_error("Cannot write temporary KTX input");
            }
            run_ktx(input, output, srgb);
            std::ifstream file(output, std::ios::binary | std::ios::ate);
            const auto length = file.tellg();
            if (!file || length <= 0 || uint64_t(length) > std::numeric_limits<uint32>::max()) throw std::runtime_error("Cannot read converted KTX payload");
            std::vector<uint8_t> bytes(static_cast<size_t>(length));
            file.seekg(0);
            file.read(reinterpret_cast<char *>(bytes.data()), length);
            if (!file) throw std::runtime_error("Truncated converted KTX payload");
            return bytes;
        }
    }

    void convert_textures_to_ktx(data &meshes) {
        if (meshes.textures.empty()) {
            meshes.images.clear();
            return;
        }
        temporary_directory temporary;
        std::vector<image> images;
        std::vector<texture> textures;
        std::map<std::pair<uint32, bool>, uint32> image_ids;
        std::map<std::pair<int32, bool>, int32> texture_ids;
        auto convert_slot = [&](texture_info &slot, bool srgb) {
            if (slot.index < 0) return;
            const auto key = std::make_pair(slot.index, srgb);
            if (const auto found = texture_ids.find(key); found != texture_ids.end()) {
                slot.index = found->second;
                return;
            }
            const auto &source = meshes.textures.at(size_t(slot.index));
            const auto image_key = std::make_pair(source.image_id, srgb);
            auto found = image_ids.find(image_key);
            if (found == image_ids.end()) {
                const auto id = static_cast<uint32>(images.size());
                images.push_back({.mime_type = "image/ktx2", .encoded_data = encode(meshes.images.at(source.image_id), temporary.directory, srgb)});
                found = image_ids.emplace(image_key, id).first;
            }
            const auto id = static_cast<int32>(textures.size());
            textures.push_back({.image_id = found->second, .sampler_id = source.sampler_id});
            texture_ids.emplace(key, id);
            slot.index = id;
        };
        // A source image used in both color and data slots needs two Vulkan formats.
        // Share each encoded variant across all texture/sampler combinations using it.
        for (auto &material : meshes.materials) {
            convert_slot(material.base_color_texture, true);
            convert_slot(material.emissive_texture, true);
            convert_slot(material.metallic_roughness_texture, false);
            convert_slot(material.normal_texture, false);
            convert_slot(material.occlusion_texture, false);
            convert_slot(material.transmission_texture, false);
            convert_slot(material.thickness_texture, false);
            convert_slot(material.specular_texture, false);
            convert_slot(material.specular_color_texture, true);
            convert_slot(material.clearcoat_texture, false);
            convert_slot(material.clearcoat_roughness_texture, false);
            convert_slot(material.clearcoat_normal_texture, false);
            convert_slot(material.sheen_color_texture, true);
            convert_slot(material.sheen_roughness_texture, false);
            convert_slot(material.anisotropy_texture, false);
            convert_slot(material.iridescence_texture, false);
            convert_slot(material.iridescence_thickness_texture, false);
        }
        meshes.images = std::move(images);
        meshes.textures = std::move(textures);
    }
}
