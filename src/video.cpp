#include <vulkanizer/video.hpp>
#include <vulkanizer/builders.hpp>
#include <vulkanizer/barrier.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include "video/clip.hpp"
#include "video/commands.hpp"
#include "video/h264_parameters.hpp"
#include "video_resolve.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <stdexcept>

namespace vkz::video {
    namespace {
        void require(bool condition, const char *message) {
            if (!condition) {
                throw std::runtime_error(message);
            }
        }

        VkDeviceSize align(VkDeviceSize size, VkDeviceSize alignment) {
            return ((size + alignment - 1) / alignment) * alignment;
        }

        VkImageSubresourceRange range(uint32_t layer = 0) {
            return {static_cast<VkImageAspectFlags>(vkz::image_aspect_color), 0, 1, layer, 1};
        }
    } // namespace

    std::span<const char *const> required_extensions() {
        static constexpr const char *extensions[]{VK_KHR_VIDEO_QUEUE_EXTENSION_NAME, VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME, VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME};
        return extensions;
    }

    uint32_t decode_queue_family(VkPhysicalDevice device) {
        uint32_t count{};
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties2> properties(count, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
        std::vector<VkQueueFamilyVideoPropertiesKHR> video(count, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR});
        for (uint32_t i = 0; i < count; ++i) {
            properties[i].pNext = &video[i];
        }
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, properties.data());
        for (uint32_t i = 0; i < count; ++i) {
            if ((properties[i].queueFamilyProperties.queueFlags & static_cast<VkQueueFlags>(vkz::queue_video_decode)) && (video[i].videoCodecOperations & static_cast<VkVideoCodecOperationFlagsKHR>(vkz::video_codec_operation_decode_h264))) {
                return i;
            }
        }
        throw std::runtime_error("No H.264 video decode queue family");
    }

    struct decoder::impl {
        struct output {
            texture rgb{};
            descriptor_set resolve_set{};
        };

        ~impl() {
            if (!device) {
                return;
            }
            // Also cover exceptions between decode submission and the synchronous resolve.
            if (params.decode_queue) {
                batch_submission{params.decode_queue}.wait_idle();
            }
            if (params.graphics_queue) {
                batch_submission{params.graphics_queue}.wait_idle();
            }
            pipeline.destroy();
            pool.destroy();
            layout.destroy();
            yuv_sampler.destroy();
            for (auto &view : yuv_views) {
                view.destroy();
            }
            for (auto &view : dpb_views) {
                view.destroy();
            }
            distinct_view.destroy();
            distinct_yuv_view.destroy();
            if (conversion) {
                vkDestroySamplerYcbcrConversion(device, conversion, nullptr);
            }
            for (auto &value : outputs) {
                value.rgb.destroy();
            }
            distinct.destroy();
            dpb.destroy();
            bitstream.destroy();
            if (parameters) {
                vkDestroyVideoSessionParametersKHR(device, parameters, nullptr);
            }
            if (session) {
                vkDestroyVideoSessionKHR(device, session, nullptr);
            }
            for (auto &allocation : allocations) {
                allocation.destroy();
            }
            if (decoded_semaphore) {
                vkDestroySemaphore(device, decoded_semaphore, nullptr);
            }
            if (resolved_semaphore) {
                vkDestroySemaphore(device, resolved_semaphore, nullptr);
            }
        }

        void init(const decoder_params &value, const clip &input) {
            require(value.allocator && value.allocator->device && input.impl_, "Video decoder needs a valid allocator and clip");
            params = value;
            device = value.allocator->device;
            source = input;
            require(params.graphics_queue && params.decode_queue, "Video decoder needs graphics and decode queues");
            require(params.decode_queue_family == video::decode_queue_family(device.physical), "Decode queue must support H.264");
            require(vkCreateVideoSessionKHR && vkCmdDecodeVideoKHR, "Vulkan Video decode extensions were not enabled");
            const auto &data = *source.impl_;
            h264_profile.stdProfileIdc = static_cast<StdVideoH264ProfileIdc>(data.sps[data.sps_ids.front()].profile_idc);
            h264_profile.pictureLayout = static_cast<VkVideoDecodeH264PictureLayoutFlagBitsKHR>(vkz::video_decode_h264_picture_layout_progressive);
            profile.pNext = &h264_profile;
            profile.videoCodecOperation = static_cast<VkVideoCodecOperationFlagBitsKHR>(vkz::video_codec_operation_decode_h264);
            profile.chromaSubsampling = static_cast<VkVideoChromaSubsamplingFlagsKHR>(vkz::video_chroma_subsampling_s420);
            profile.lumaBitDepth = static_cast<VkVideoComponentBitDepthFlagsKHR>(vkz::video_component_bit_depth_bits_8);
            profile.chromaBitDepth = static_cast<VkVideoComponentBitDepthFlagsKHR>(vkz::video_component_bit_depth_bits_8);
            profiles.profileCount = 1;
            profiles.pProfiles = &profile;
            capabilities.pNext = &decode_capabilities;
            decode_capabilities.pNext = &h264_capabilities;
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceVideoCapabilitiesKHR(device.physical, &profile, &capabilities));
            slot_count = data.max_references + 1;
            require(slot_count <= capabilities.maxDpbSlots && data.max_references <= capabilities.maxActiveReferencePictures, "Video exceeds device DPB capacity");
            extent = {data.coded_width, data.coded_height};
            require(extent.width >= capabilities.minCodedExtent.width && extent.height >= capabilities.minCodedExtent.height && extent.width <= capabilities.maxCodedExtent.width &&
                        extent.height <= capabilities.maxCodedExtent.height,
                    "Video coded extent is unsupported");
            coincide = (decode_capabilities.flags & static_cast<VkVideoDecodeCapabilityFlagsKHR>(vkz::video_decode_capability_dpb_and_output_coincide)) != 0;
            require(coincide || (decode_capabilities.flags & static_cast<VkVideoDecodeCapabilityFlagsKHR>(vkz::video_decode_capability_dpb_and_output_distinct)), "Device supports no usable decode output mode");
            dpb_format = choose_format(vkz::image_usage_video_decode_dpb | (coincide ? vkz::image_usage_video_decode_dst | vkz::image_usage_sampled : vkz::image_usage_none));
            output_format = coincide ? dpb_format : choose_format(vkz::image_usage_video_decode_dst | vkz::image_usage_sampled);
            create_session();
            create_parameters();
            create_resources();
            decode_pool.init(device, params.decode_queue_family, vkz::command_pool_flags_transient, params.decode_queue);
            graphics_pool.init(device, params.graphics_queue_family, vkz::command_pool_flags_transient, params.graphics_queue);
            decoded_semaphore = create_semaphore(device);
            resolved_semaphore = create_semaphore(device);
        }

        VkVideoFormatPropertiesKHR choose_format(vkz::image_usage usage) {
            VkPhysicalDeviceVideoFormatInfoKHR info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR};
            info.pNext = &profiles;
            info.imageUsage = static_cast<VkImageUsageFlags>(usage);
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceVideoFormatPropertiesKHR(device.physical, &info, &count, nullptr));
            std::vector<VkVideoFormatPropertiesKHR> formats(count, {VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR});
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceVideoFormatPropertiesKHR(device.physical, &info, &count, formats.data()));
            const auto it = std::find_if(formats.begin(), formats.end(), [](const auto &f) {
                return f.format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM && f.imageType == VK_IMAGE_TYPE_2D && f.imageTiling == VK_IMAGE_TILING_OPTIMAL;
            });
            require(it != formats.end(), "Device lacks a compatible 8-bit NV12 decode format");
            return *it;
        }

        void create_session() {
            VkVideoSessionCreateInfoKHR info{VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR};
            info.queueFamilyIndex = params.decode_queue_family;
            info.pVideoProfile = &profile;
            info.pictureFormat = output_format.format;
            info.maxCodedExtent = extent;
            info.referencePictureFormat = dpb_format.format;
            info.maxDpbSlots = slot_count;
            info.maxActiveReferencePictures = source.impl_->max_references;
            info.pStdHeaderVersion = &capabilities.stdHeaderVersion;
            VKZ_CHECK_VULKAN(vkCreateVideoSessionKHR(device, &info, nullptr, &session));
            uint32_t count{};
            VKZ_CHECK_VULKAN(vkGetVideoSessionMemoryRequirementsKHR(device, session, &count, nullptr));
            std::vector<VkVideoSessionMemoryRequirementsKHR> requirements(count, {VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR});
            VKZ_CHECK_VULKAN(vkGetVideoSessionMemoryRequirementsKHR(device, session, &count, requirements.data()));
            std::vector<VkBindVideoSessionMemoryInfoKHR> bindings;
            for (const auto &requirement : requirements) {
                allocations.push_back(device_memory::allocate(*params.allocator, requirement.memoryRequirements));
                const auto &memory = allocations.back();
                VkBindVideoSessionMemoryInfoKHR binding{VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR};
                binding.memoryBindIndex = requirement.memoryBindIndex;
                binding.memory = memory.handle;
                binding.memoryOffset = memory.offset;
                binding.memorySize = memory.size;
                bindings.push_back(binding);
            }
            VKZ_CHECK_VULKAN(vkBindVideoSessionMemoryKHR(device, session, static_cast<uint32_t>(bindings.size()), bindings.data()));
        }

        void create_parameters() {
            const auto &data = *source.impl_;
            std::vector<StdVideoH264SequenceParameterSet> sps(data.sps_ids.size());
            std::vector<StdVideoH264SequenceParameterSetVui> vui(sps.size());
            std::vector<StdVideoH264HrdParameters> hrd(sps.size());
            std::vector<StdVideoH264ScalingLists> sps_scaling(sps.size());
            for (size_t i = 0; i < sps.size(); ++i) {
                const auto &parsed = data.sps[data.sps_ids[i]];
                translate(parsed, sps[i], vui[i], hrd[i]);
                if (parsed.seq_scaling_matrix_present_flag) {
                    auto &scaling = sps_scaling[i];
                    for (uint32_t j = 0; j < 8; ++j) {
                        scaling.scaling_list_present_mask |= parsed.seq_scaling_list_present_flag[j] << j;
                    }
                    for (uint32_t j = 0; j < 6; ++j) {
                        scaling.use_default_scaling_matrix_mask |= parsed.UseDefaultScalingMatrix4x4Flag[j] << j;
                        for (uint32_t k = 0; k < 16; ++k) {
                            scaling.ScalingList4x4[j][k] = static_cast<uint8_t>(parsed.ScalingList4x4[j][k]);
                        }
                    }
                    for (uint32_t j = 0; j < 2; ++j) {
                        scaling.use_default_scaling_matrix_mask |= parsed.UseDefaultScalingMatrix8x8Flag[j] << (j + 6);
                        for (uint32_t k = 0; k < 64; ++k) {
                            scaling.ScalingList8x8[j][k] = static_cast<uint8_t>(parsed.ScalingList8x8[j][k]);
                        }
                    }
                    sps[i].pScalingLists = &scaling;
                }
                require(sps[i].level_idc <= h264_capabilities.maxLevelIdc, "H.264 level exceeds device capabilities");
            }
            std::vector<StdVideoH264PictureParameterSet> pps(data.pps_ids.size());
            std::vector<StdVideoH264ScalingLists> scaling(pps.size());
            for (size_t i = 0; i < pps.size(); ++i) {
                translate(data.pps[data.pps_ids[i]], pps[i], scaling[i]);
            }
            VkVideoDecodeH264SessionParametersAddInfoKHR add{VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR};
            add.stdSPSCount = static_cast<uint32_t>(sps.size());
            add.pStdSPSs = sps.data();
            add.stdPPSCount = static_cast<uint32_t>(pps.size());
            add.pStdPPSs = pps.data();
            VkVideoDecodeH264SessionParametersCreateInfoKHR h264{VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR};
            h264.maxStdSPSCount = add.stdSPSCount;
            h264.maxStdPPSCount = add.stdPPSCount;
            h264.pParametersAddInfo = &add;
            VkVideoSessionParametersCreateInfoKHR info{VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR};
            info.videoSession = session;
            info.pNext = &h264;
            VKZ_CHECK_VULKAN(vkCreateVideoSessionParametersKHR(device, &info, nullptr, &parameters));
        }

        image make_decode_image(const VkVideoFormatPropertiesKHR &format, vkz::image_usage usage, uint32_t layers) {
            auto builder = image::builder(*params.allocator);
            builder.flags(static_cast<vkz::image_flags>(format.imageCreateFlags))
                .type(format.imageType)
                .format(format.format)
                .extent(extent.width, extent.height)
                .tiling(format.imageTiling)
                .usage(usage)
                .array_layers(layers)
                .next(&profiles);
            std::array<uint32_t, 2> families{params.decode_queue_family, params.graphics_queue_family};
            if (families[0] != families[1]) {
                builder.sharing_mode(VK_SHARING_MODE_CONCURRENT).queue_families(families.data(), 2);
            }
            return builder.build();
        }

        void create_resources() {
            const auto &data = *source.impl_;
            for (const auto &unit : data.units) {
                const auto offset = align(stream_size, capabilities.minBitstreamBufferOffsetAlignment);
                const auto size = align(unit.bytes.size(), capabilities.minBitstreamBufferSizeAlignment);
                offsets.push_back(offset);
                sizes.push_back(size);
                stream_size = offset + size;
            }
            bitstream = buffer::builder(*params.allocator)
                .size(stream_size)
                .usage(vkz::buffer_usage_video_decode_src)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .next(&profiles)
                .build();
            auto mapping = bitstream.map();
            require(mapping._, "Cannot map video bitstream");
            std::memset(mapping._, 0, stream_size);
            for (size_t i = 0; i < data.units.size(); ++i) {
                std::memcpy(mapping.as<uint8_t>() + offsets[i], data.units[i].bytes.data(), data.units[i].bytes.size());
            }
            bitstream.flush();
            mapping.unmap();
            dpb = make_decode_image(dpb_format, vkz::image_usage_video_decode_dpb | (coincide ? vkz::image_usage_video_decode_dst | vkz::image_usage_sampled : vkz::image_usage_none),
                                    slot_count);
            if (!coincide) {
                distinct = make_decode_image(output_format, vkz::image_usage_video_decode_dst | vkz::image_usage_sampled, 1);
                VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
                usage.usage = static_cast<VkImageUsageFlags>(vkz::image_usage_video_decode_dst);
                distinct_view = image_view::builder(device).image(distinct).next(&usage).build();
            }

            VkFormatProperties format_properties{};
            vkGetPhysicalDeviceFormatProperties(device.physical, output_format.format, &format_properties);
            const auto features = format_properties.optimalTilingFeatures;
            const auto &sps = data.sps[data.sps_ids.front()];
            VkSamplerYcbcrConversionCreateInfo conversion_info{VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO};
            conversion_info.format = output_format.format;
            conversion_info.ycbcrModel =
                sps.vui.colour_description_present_flag && sps.vui.matrix_coefficients == 1 ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709 : VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;
            conversion_info.ycbcrRange = sps.vui.video_full_range_flag ? VK_SAMPLER_YCBCR_RANGE_ITU_FULL : VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;
            conversion_info.components = output_format.componentMapping;
            conversion_info.xChromaOffset = (features & static_cast<VkFormatFeatureFlags>(vkz::format_feature_cosited_chroma_samples)) ? VK_CHROMA_LOCATION_COSITED_EVEN : VK_CHROMA_LOCATION_MIDPOINT;
            conversion_info.yChromaOffset = (features & static_cast<VkFormatFeatureFlags>(vkz::format_feature_midpoint_chroma_samples)) ? VK_CHROMA_LOCATION_MIDPOINT : VK_CHROMA_LOCATION_COSITED_EVEN;
            conversion_info.chromaFilter = (features & static_cast<VkFormatFeatureFlags>(vkz::format_feature_sampled_image_ycbcr_conversion_linear_filter)) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            VKZ_CHECK_VULKAN(vkCreateSamplerYcbcrConversion(device, &conversion_info, nullptr, &conversion));
            VkSamplerYcbcrConversionInfo conversion_next{VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO};
            conversion_next.conversion = conversion;
            yuv_sampler = sampler::builder(device)
                              .mag_filter(conversion_info.chromaFilter)
                              .min_filter(conversion_info.chromaFilter)
                              .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                              .next(&conversion_next)
                              .build();
            for (uint32_t i = 0; i < slot_count; ++i) {
                VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
                usage.usage = static_cast<VkImageUsageFlags>(vkz::image_usage_video_decode_dpb) | (coincide ? static_cast<VkImageUsageFlags>(vkz::image_usage_video_decode_dst) : 0);
                dpb_views.push_back(image_view::builder(device).image(dpb).base_array_layer(i).layer_count(1).next(&usage).build());
                if (coincide) {
                    yuv_views.push_back(image_view::builder(device).image(dpb).base_array_layer(i).layer_count(1).next(&conversion_next).build());
                }
            }
            if (!coincide) {
                distinct_yuv_view = image_view::builder(device).image(distinct).next(&conversion_next).build();
            }
            layout = make_descriptor_set_layout_builder(device)
                         .binding(0)
                         .descriptor_type(vkz::descriptor_type_combined_image_sampler)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage_compute)
                         .immutable_sampler(yuv_sampler)
                         .binding(1)
                         .descriptor_type(vkz::descriptor_type_storage_image)
                         .descriptor_count(1)
                         .shader_stages(vkz::shader_stage_compute)
                         .create_layout();
            VkSamplerYcbcrConversionImageFormatProperties conversion_properties{VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES};
            VkImageFormatProperties2 image_properties{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
            image_properties.pNext = &conversion_properties;
            VkPhysicalDeviceImageFormatInfo2 image_info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
            image_info.pNext = &profiles;
            image_info.format = output_format.format;
            image_info.type = VK_IMAGE_TYPE_2D;
            image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_info.usage = static_cast<VkImageUsageFlags>(vkz::image_usage_sampled) | static_cast<VkImageUsageFlags>(vkz::image_usage_video_decode_dst) | (coincide ? static_cast<VkImageUsageFlags>(vkz::image_usage_video_decode_dpb) : 0);
            image_info.flags = output_format.imageCreateFlags;
            VKZ_CHECK_VULKAN(vkGetPhysicalDeviceImageFormatProperties2(device.physical, &image_info, &image_properties));
            // A converted combined image sampler may consume multiple descriptors.
            pool = descriptor_pool{device,
                                   64,
                                   {{static_cast<VkDescriptorType>(vkz::descriptor_type_combined_image_sampler), 64 * conversion_properties.combinedImageSamplerDescriptorCount},
                                    {static_cast<VkDescriptorType>(vkz::descriptor_type_storage_image), 64}}};
            pipeline = make_compute_pipeline_builder(device)
                           .shader_stage()
                           .compute_shader(std::vector<uint32_t>(std::begin(video_resolve), std::end(video_resolve)))
                           .layout()
                           .add_descriptor_set_layout(layout)
                           .add_push_constant_range(vkz::shader_stage_compute, 0, sizeof(glm::ivec4))
                           .build();
            slot_layouts.resize(slot_count, VK_IMAGE_LAYOUT_UNDEFINED);
            frame_numbers.resize(slot_count);
            pocs.resize(slot_count);
        }

        uint32_t allocate_output() {
            require(outputs.size() < 64, "H.264 reorder buffer exceeds 64 pictures");
            const auto index = static_cast<uint32_t>(outputs.size());
            auto &output = outputs.emplace_back();
            output.rgb.image = image::builder(*params.allocator)
                .format(VK_FORMAT_R8G8B8A8_UNORM)
                .extent(source.width(), source.height())
                .usage(vkz::image_usage_sampled_storage_download)
                .build();
            output.rgb.image_view = image_view::build(device, output.rgb.image);
            output.rgb.sampler = sampler::builder(device).min_filter(VK_FILTER_LINEAR).mag_filter(VK_FILTER_LINEAR).address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE).build();
            output.resolve_set = pool.allocate(layout);
            return index;
        }

        void decode_one(uint32_t index) {
            const auto &data = *source.impl_;
            const auto &unit = data.units[index];
            const auto &info = data.frames[index];
            const auto &slice = unit.slice;
            const auto &pps = data.pps[slice.pic_parameter_set_id];
            if (info.idr) {
                references.clear();
            }
            uint32_t slot = 0;
            while (std::find(references.begin(), references.end(), slot) != references.end()) {
                ++slot;
            }
            require(slot < slot_count, "No free decoded picture buffer slot");
            std::vector<VkVideoPictureResourceInfoKHR> pictures(slot_count, {VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR});
            std::vector<StdVideoDecodeH264ReferenceInfo> reference_info(slot_count);
            std::vector<VkVideoDecodeH264DpbSlotInfoKHR> h264_slots(slot_count, {VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR});
            std::vector<VkVideoReferenceSlotInfoKHR> slots(slot_count, {VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR});
            for (uint32_t i = 0; i < slot_count; ++i) {
                pictures[i].codedExtent = extent;
                pictures[i].imageViewBinding = dpb_views[i];
                reference_info[i].FrameNum = frame_numbers[i];
                reference_info[i].PicOrderCnt[0] = pocs[i];
                reference_info[i].PicOrderCnt[1] = pocs[i];
                h264_slots[i].pStdReferenceInfo = &reference_info[i];
                slots[i].slotIndex = i;
                slots[i].pPictureResource = &pictures[i];
                slots[i].pNext = &h264_slots[i];
            }
            std::vector<VkVideoReferenceSlotInfoKHR> bound;
            for (auto reference : references) {
                bound.push_back(slots[reference]);
            }
            bound.push_back(slots[slot]);
            bound.back().slotIndex = -1;
            reference_info[slot].FrameNum = slice.frame_num;
            reference_info[slot].PicOrderCnt[0] = info.picture_order_count;
            reference_info[slot].PicOrderCnt[1] = info.picture_order_count;
            auto command = decode_pool.create_command_buffer();
            // Resolve's semaphore protects any layer returning from shader reads.
            for (auto i : references) {
                barrier::push(dpb.handle, range(i), vkz::pipeline_stage_video_decode, vkz::pipeline_stage_video_decode, vkz::access_video_decode_read_write, vkz::access_video_decode_read,
                              slot_layouts[i], VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR);
                slot_layouts[i] = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
            }
            barrier::push(dpb.handle, range(slot), vkz::pipeline_stage_video_decode, vkz::pipeline_stage_video_decode, vkz::access_video_decode_read_write, vkz::access_video_decode_write,
                          slot_layouts[slot], VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR);
            slot_layouts[slot] = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
            if (!coincide) {
                barrier::push(distinct, range(), vkz::pipeline_stage_none, vkz::pipeline_stage_video_decode, vkz::access_none, vkz::access_video_decode_write,
                              VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR);
            }
            barrier::flush(command);
            begin_coding(command, session, parameters, bound);
            if (index == 0) {
                reset_coding(command);
            }
            StdVideoDecodeH264PictureInfo standard{};
            standard.flags.is_intra = slice.slice_type % 5 == 2;
            standard.flags.is_reference = info.reference;
            standard.flags.IdrPicFlag = info.idr;
            standard.pic_parameter_set_id = slice.pic_parameter_set_id;
            standard.seq_parameter_set_id = pps.seq_parameter_set_id;
            standard.frame_num = slice.frame_num;
            standard.idr_pic_id = slice.idr_pic_id;
            standard.PicOrderCnt[0] = info.picture_order_count;
            standard.PicOrderCnt[1] = info.picture_order_count;
            VkVideoDecodeH264PictureInfoKHR h264{VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR};
            h264.pStdPictureInfo = &standard;
            h264.sliceCount = static_cast<uint32_t>(unit.slice_offsets.size());
            h264.pSliceOffsets = unit.slice_offsets.data();
            VkVideoDecodeInfoKHR decode{VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR};
            decode.pNext = &h264;
            decode.srcBuffer = bitstream;
            decode.srcBufferOffset = offsets[index];
            decode.srcBufferRange = sizes[index];
            decode.dstPictureResource = pictures[slot];
            if (!coincide) {
                decode.dstPictureResource.imageViewBinding = distinct_view;
            }
            decode.pSetupReferenceSlot = &slots[slot];
            decode.referenceSlotCount = static_cast<uint32_t>(references.size());
            decode.pReferenceSlots = references.empty() ? nullptr : bound.data();
            decode_picture(command, decode);
            end_coding(command);
            cmd_end(command);
            submission_batch batch;
            batch.enqueue(1, &command);
            if (resolve_pending) {
                batch.enqueue_wait(resolved_semaphore, 0, vkz::pipeline_stage_all_commands);
            }
            batch.enqueue_signal(decoded_semaphore);
            batch_submission submit{params.decode_queue};
            submit.enqueue_batch(std::move(batch));
            VKZ_CHECK_VULKAN(submit.execute());

            const auto output_index = free_outputs.empty() ? allocate_output() : free_outputs.back();
            if (!free_outputs.empty()) {
                free_outputs.pop_back();
            }
            auto &output = outputs[output_index];
            const auto sampled = coincide ? yuv_views[slot] : distinct_yuv_view;
            update_descriptor(device, {output.resolve_set,
                                       {texture_descriptor{sampled, yuv_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0}, image_descriptor{output.rgb.image_view, VK_IMAGE_LAYOUT_GENERAL, 1}}});
            auto resolve = graphics_pool.create_command_buffer();
            auto &decoded = coincide ? dpb : distinct;
            const auto old_layout = coincide ? slot_layouts[slot] : distinct.layout;
            barrier::push(decoded.handle, range(coincide ? slot : 0), vkz::pipeline_stage_none, vkz::pipeline_stage_compute_shader, vkz::access_none, vkz::access_shader_sampled_read, old_layout,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (coincide) {
                slot_layouts[slot] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else {
                distinct.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            barrier::push(output.rgb.image, range(), vkz::pipeline_stage_all_commands, vkz::pipeline_stage_compute_shader, vkz::access_shader_sampled_read, vkz::access_shader_storage_write,
                          VK_IMAGE_LAYOUT_GENERAL);
            barrier::flush(resolve);
            bind_pipeline(resolve, pipeline);
            bind_descriptor_sets(resolve, pipeline, {output.resolve_set});
            const glm::ivec4 dimensions{source.width(), source.height(), extent.width, extent.height};
            push_constants(resolve, pipeline, dimensions, vkz::shader_stage_compute);
            dispatch(resolve, (source.width() + 15) / 16, (source.height() + 15) / 16);
            barrier::push_and_flush(resolve, output.rgb.image, range(), vkz::pipeline_stage_compute_shader, vkz::pipeline_stage_all_commands, vkz::access_shader_storage_write,
                                    vkz::access_shader_sampled_read, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            cmd_end(resolve);
            submission_batch graphics_batch;
            graphics_batch.enqueue(1, &resolve);
            graphics_batch.enqueue_wait(decoded_semaphore, 0, vkz::pipeline_stage_all_commands);
            graphics_batch.enqueue_signal(resolved_semaphore);
            batch_submission graphics_submit{params.graphics_queue};
            graphics_submit.enqueue_batch(std::move(graphics_batch));
            VKZ_CHECK_VULKAN(graphics_submit.execute());
            graphics_submit.wait_idle();
            decode_pool.destroy(command);
            graphics_pool.destroy(resolve);
            resolve_pending = true;
            buffered.emplace(info.display_order, output_index);
            decoded_indices.push_back(index);
            frame_numbers[slot] = slice.frame_num;
            pocs[slot] = info.picture_order_count;
            if (info.reference) {
                // MMCO is applied after this picture has consumed its references.
                if (slice.drpm.adaptive_ref_pic_marking_mode_flag) {
                    const auto max_frame_num = 1 << (data.sps[pps.seq_parameter_set_id].log2_max_frame_num_minus4 + 4);
                    for (size_t i = 0; i < std::size(slice.drpm.memory_management_control_operation); ++i) {
                        const auto operation = slice.drpm.memory_management_control_operation[i];
                        if (operation == 0) {
                            break;
                        }
                        const auto removed = (slice.frame_num - slice.drpm.difference_of_pic_nums_minus1[i] - 1 + max_frame_num) % max_frame_num;
                        std::erase_if(references, [&](uint32_t ref) {
                            return frame_numbers[ref] == removed;
                        });
                    }
                }
                references.push_back(slot);
                if (references.size() > source.impl_->max_references) {
                    references.erase(references.begin());
                }
            }
        }

        const texture &frame(uint32_t order) {
            require(order < source.frames().size(), "Video frame order is out of range");
            if (last_order != UINT32_MAX && order < last_order) {
                reset();
            }
            for (auto it = buffered.begin(); it != buffered.end() && it->first < order;) {
                free_outputs.push_back(it->second);
                it = buffered.erase(it);
            }
            while (!buffered.contains(order)) {
                require(next_decode < source.frames().size(), "Decode reached EOF without the requested frame");
                decode_one(next_decode++);
                // A catch-up decode need not retain old RGB frames.
                for (auto it = buffered.begin(); it != buffered.end() && it->first < order;) {
                    free_outputs.push_back(it->second);
                    it = buffered.erase(it);
                }
            }
            last_order = order;
            return outputs[buffered.at(order)].rgb;
        }

        void reset() {
            for (const auto &[order, index] : buffered) {
                free_outputs.push_back(index);
            }
            buffered.clear();
            references.clear();
            decoded_indices.clear();
            next_decode = 0;
            last_order = UINT32_MAX;
        }

        decoder_params params{};
        vkz::device device{};
        clip source;
        VkVideoDecodeH264ProfileInfoKHR h264_profile{VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR};
        VkVideoProfileInfoKHR profile{VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR};
        VkVideoProfileListInfoKHR profiles{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
        VkVideoCapabilitiesKHR capabilities{VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR};
        VkVideoDecodeCapabilitiesKHR decode_capabilities{VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR};
        VkVideoDecodeH264CapabilitiesKHR h264_capabilities{VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR};
        VkVideoFormatPropertiesKHR dpb_format{};
        VkVideoFormatPropertiesKHR output_format{};
        VkVideoSessionKHR session{};
        VkVideoSessionParametersKHR parameters{};
        std::vector<device_memory> allocations;
        VkSamplerYcbcrConversion conversion{};
        sampler yuv_sampler{};
        buffer bitstream{};
        image dpb{};
        image distinct{};
        image_view distinct_view{};
        image_view distinct_yuv_view{};
        std::vector<image_view> dpb_views;
        std::vector<image_view> yuv_views;
        std::vector<output> outputs;
        std::vector<uint32_t> free_outputs;
        std::map<uint32_t, uint32_t> buffered;
        std::vector<uint32_t> references;
        std::vector<int32_t> frame_numbers;
        std::vector<int32_t> pocs;
        std::vector<VkImageLayout> slot_layouts;
        std::vector<VkDeviceSize> offsets;
        std::vector<VkDeviceSize> sizes;
        std::vector<uint32_t> decoded_indices;
        command_pool decode_pool;
        command_pool graphics_pool;
        descriptor_pool pool;
        descriptor_set_layout layout{};
        vkz::pipeline pipeline{};
        VkSemaphore decoded_semaphore{};
        VkSemaphore resolved_semaphore{};
        VkExtent2D extent{};
        VkDeviceSize stream_size{};
        uint32_t slot_count{};
        uint32_t next_decode{};
        uint32_t last_order{UINT32_MAX};
        bool coincide{};
        bool resolve_pending{};
    };

    decoder::decoder(const decoder_params &params, const clip &clip) : impl_(std::make_unique<impl>()) {
        impl_->init(params, clip);
    }

    decoder::~decoder() = default;

    const texture &decoder::frame(uint32_t display_order) {
        return impl_->frame(display_order);
    }

    void decoder::reset() {
        impl_->reset();
    }

    std::span<const uint32_t> decoder::decoded_frames() const {
        return impl_->decoded_indices;
    }

    const clip &decoder::source() const {
        return impl_->source;
    }
} // namespace vkz::video
