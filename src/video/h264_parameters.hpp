#pragma once
#include "third_party/h264.hpp"
#include <vulkanizer/vkz.hpp>
#include <iterator>
#include <stdexcept>

namespace vkz::video {
    inline void translate(const h264::SPS &sps, StdVideoH264SequenceParameterSet &vk_sps, StdVideoH264SequenceParameterSetVui &vk_vui, StdVideoH264HrdParameters &vk_hrd) {
        vk_sps.flags.constraint_set0_flag = sps.constraint_set0_flag;
        vk_sps.flags.constraint_set1_flag = sps.constraint_set1_flag;
        vk_sps.flags.constraint_set2_flag = sps.constraint_set2_flag;
        vk_sps.flags.constraint_set3_flag = sps.constraint_set3_flag;
        vk_sps.flags.constraint_set4_flag = sps.constraint_set4_flag;
        vk_sps.flags.constraint_set5_flag = sps.constraint_set5_flag;
        vk_sps.flags.direct_8x8_inference_flag = sps.direct_8x8_inference_flag;
        vk_sps.flags.mb_adaptive_frame_field_flag = sps.mb_adaptive_frame_field_flag;
        vk_sps.flags.frame_mbs_only_flag = sps.frame_mbs_only_flag;
        vk_sps.flags.delta_pic_order_always_zero_flag = sps.delta_pic_order_always_zero_flag;
        vk_sps.flags.separate_colour_plane_flag = sps.separate_colour_plane_flag;
        vk_sps.flags.gaps_in_frame_num_value_allowed_flag = sps.gaps_in_frame_num_value_allowed_flag;
        vk_sps.flags.qpprime_y_zero_transform_bypass_flag = sps.qpprime_y_zero_transform_bypass_flag;
        vk_sps.flags.frame_cropping_flag = sps.frame_cropping_flag;
        vk_sps.flags.seq_scaling_matrix_present_flag = sps.seq_scaling_matrix_present_flag;
        vk_sps.flags.vui_parameters_present_flag = sps.vui_parameters_present_flag;

        if (vk_sps.flags.vui_parameters_present_flag) {
            vk_sps.pSequenceParameterSetVui = &vk_vui;
            vk_vui.flags.aspect_ratio_info_present_flag = sps.vui.aspect_ratio_info_present_flag;
            vk_vui.flags.overscan_info_present_flag = sps.vui.overscan_info_present_flag;
            vk_vui.flags.overscan_appropriate_flag = sps.vui.overscan_appropriate_flag;
            vk_vui.flags.video_signal_type_present_flag = sps.vui.video_signal_type_present_flag;
            vk_vui.flags.video_full_range_flag = sps.vui.video_full_range_flag;
            vk_vui.flags.color_description_present_flag = sps.vui.colour_description_present_flag;
            vk_vui.flags.chroma_loc_info_present_flag = sps.vui.chroma_loc_info_present_flag;
            vk_vui.flags.timing_info_present_flag = sps.vui.timing_info_present_flag;
            vk_vui.flags.fixed_frame_rate_flag = sps.vui.fixed_frame_rate_flag;
            vk_vui.flags.bitstream_restriction_flag = sps.vui.bitstream_restriction_flag;
            vk_vui.flags.nal_hrd_parameters_present_flag = sps.vui.nal_hrd_parameters_present_flag;
            vk_vui.flags.vcl_hrd_parameters_present_flag = sps.vui.vcl_hrd_parameters_present_flag;

            vk_vui.aspect_ratio_idc = (StdVideoH264AspectRatioIdc)sps.vui.aspect_ratio_idc;
            vk_vui.sar_width = sps.vui.sar_width;
            vk_vui.sar_height = sps.vui.sar_height;
            vk_vui.video_format = sps.vui.video_format;
            vk_vui.colour_primaries = sps.vui.colour_primaries;
            vk_vui.transfer_characteristics = sps.vui.transfer_characteristics;
            vk_vui.matrix_coefficients = sps.vui.matrix_coefficients;
            vk_vui.num_units_in_tick = sps.vui.num_units_in_tick;
            vk_vui.time_scale = sps.vui.time_scale;
            vk_vui.max_num_reorder_frames = sps.vui.num_reorder_frames;
            vk_vui.max_dec_frame_buffering = sps.vui.max_dec_frame_buffering;
            vk_vui.chroma_sample_loc_type_top_field = sps.vui.chroma_sample_loc_type_top_field;
            vk_vui.chroma_sample_loc_type_bottom_field = sps.vui.chroma_sample_loc_type_bottom_field;

            vk_vui.pHrdParameters = &vk_hrd;
            vk_hrd.cpb_cnt_minus1 = sps.hrd.cpb_cnt_minus1;
            vk_hrd.bit_rate_scale = sps.hrd.bit_rate_scale;
            vk_hrd.cpb_size_scale = sps.hrd.cpb_size_scale;
            for (int j = 0; j < std::size(sps.hrd.bit_rate_value_minus1); ++j) {
                vk_hrd.bit_rate_value_minus1[j] = sps.hrd.bit_rate_value_minus1[j];
                vk_hrd.cpb_size_value_minus1[j] = sps.hrd.cpb_size_value_minus1[j];
                vk_hrd.cbr_flag[j] = sps.hrd.cbr_flag[j];
            }
            vk_hrd.initial_cpb_removal_delay_length_minus1 = sps.hrd.initial_cpb_removal_delay_length_minus1;
            vk_hrd.cpb_removal_delay_length_minus1 = sps.hrd.cpb_removal_delay_length_minus1;
            vk_hrd.dpb_output_delay_length_minus1 = sps.hrd.dpb_output_delay_length_minus1;
            vk_hrd.time_offset_length = sps.hrd.time_offset_length;
        }

        vk_sps.profile_idc = (StdVideoH264ProfileIdc)sps.profile_idc;
        switch (sps.level_idc) {
        case 10:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_0;
            break;
        case 11:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_1;
            break;
        case 12:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_2;
            break;
        case 13:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_1_3;
            break;
        case 20:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_2_0;
            break;
        case 21:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_2_1;
            break;
        case 22:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_2_2;
            break;
        case 30:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_3_0;
            break;
        case 31:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_3_1;
            break;
        case 32:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_3_2;
            break;
        case 40:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_4_0;
            break;
        case 41:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_4_1;
            break;
        case 42:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_4_2;
            break;
        case 50:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_5_0;
            break;
        case 51:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_5_1;
            break;
        case 52:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_5_2;
            break;
        case 60:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_6_0;
            break;
        case 61:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_6_1;
            break;
        case 62:
            vk_sps.level_idc = STD_VIDEO_H264_LEVEL_IDC_6_2;
            break;
        default:
            throw std::runtime_error("Unsupported H.264 level");
        }
        vk_sps.chroma_format_idc = (StdVideoH264ChromaFormatIdc)sps.chroma_format_idc;
        vk_sps.seq_parameter_set_id = sps.seq_parameter_set_id;
        vk_sps.bit_depth_luma_minus8 = sps.bit_depth_luma_minus8;
        vk_sps.bit_depth_chroma_minus8 = sps.bit_depth_chroma_minus8;
        vk_sps.log2_max_frame_num_minus4 = sps.log2_max_frame_num_minus4;
        vk_sps.pic_order_cnt_type = (StdVideoH264PocType)sps.pic_order_cnt_type;
        vk_sps.offset_for_non_ref_pic = sps.offset_for_non_ref_pic;
        vk_sps.offset_for_top_to_bottom_field = sps.offset_for_top_to_bottom_field;
        vk_sps.log2_max_pic_order_cnt_lsb_minus4 = sps.log2_max_pic_order_cnt_lsb_minus4;
        vk_sps.num_ref_frames_in_pic_order_cnt_cycle = sps.num_ref_frames_in_pic_order_cnt_cycle;
        vk_sps.max_num_ref_frames = sps.num_ref_frames;
        vk_sps.pic_width_in_mbs_minus1 = sps.pic_width_in_mbs_minus1;
        vk_sps.pic_height_in_map_units_minus1 = sps.pic_height_in_map_units_minus1;
        vk_sps.frame_crop_left_offset = sps.frame_crop_left_offset;
        vk_sps.frame_crop_right_offset = sps.frame_crop_right_offset;
        vk_sps.frame_crop_top_offset = sps.frame_crop_top_offset;
        vk_sps.frame_crop_bottom_offset = sps.frame_crop_bottom_offset;
        vk_sps.pOffsetForRefFrame = sps.offset_for_ref_frame;
    }

    inline void translate(const h264::PPS &pps, StdVideoH264PictureParameterSet &vk_pps, StdVideoH264ScalingLists &vk_scalinglist) {
        vk_pps.flags.transform_8x8_mode_flag = pps.transform_8x8_mode_flag;
        vk_pps.flags.redundant_pic_cnt_present_flag = pps.redundant_pic_cnt_present_flag;
        vk_pps.flags.constrained_intra_pred_flag = pps.constrained_intra_pred_flag;
        vk_pps.flags.deblocking_filter_control_present_flag = pps.deblocking_filter_control_present_flag;
        vk_pps.flags.weighted_pred_flag = pps.weighted_pred_flag;
        vk_pps.flags.bottom_field_pic_order_in_frame_present_flag = pps.pic_order_present_flag;
        vk_pps.flags.entropy_coding_mode_flag = pps.entropy_coding_mode_flag;
        vk_pps.flags.pic_scaling_matrix_present_flag = pps.pic_scaling_matrix_present_flag;

        vk_pps.seq_parameter_set_id = pps.seq_parameter_set_id;
        vk_pps.pic_parameter_set_id = pps.pic_parameter_set_id;
        vk_pps.num_ref_idx_l0_default_active_minus1 = pps.num_ref_idx_l0_active_minus1;
        vk_pps.num_ref_idx_l1_default_active_minus1 = pps.num_ref_idx_l1_active_minus1;
        vk_pps.weighted_bipred_idc = static_cast<StdVideoH264WeightedBipredIdc>(pps.weighted_bipred_idc);
        vk_pps.pic_init_qp_minus26 = static_cast<int8_t>(pps.pic_init_qp_minus26);
        vk_pps.pic_init_qs_minus26 = static_cast<int8_t>(pps.pic_init_qs_minus26);
        vk_pps.chroma_qp_index_offset = static_cast<int8_t>(pps.chroma_qp_index_offset);
        vk_pps.second_chroma_qp_index_offset = static_cast<int8_t>(pps.second_chroma_qp_index_offset);

        vk_pps.pScalingLists = pps.pic_scaling_matrix_present_flag ? &vk_scalinglist : nullptr;
        for (int j = 0; j < std::size(pps.pic_scaling_list_present_flag); ++j) {
            vk_scalinglist.scaling_list_present_mask |= pps.pic_scaling_list_present_flag[j] << j;
        }
        for (int j = 0; j < std::size(pps.UseDefaultScalingMatrix4x4Flag); ++j) {
            vk_scalinglist.use_default_scaling_matrix_mask |= pps.UseDefaultScalingMatrix4x4Flag[j] << j;
        }
        for (int j = 0; j < std::size(pps.ScalingList4x4); ++j) {
            for (int k = 0; k < std::size(pps.ScalingList4x4[j]); ++k) {
                vk_scalinglist.ScalingList4x4[j][k] = (uint8_t)pps.ScalingList4x4[j][k];
            }
        }
        for (int j = 0; j < std::size(pps.ScalingList8x8); ++j) {
            for (int k = 0; k < std::size(pps.ScalingList8x8[j]); ++k) {
                vk_scalinglist.ScalingList8x8[j][k] = (uint8_t)pps.ScalingList8x8[j][k];
            }
        }
    }

} // namespace vkz::video
