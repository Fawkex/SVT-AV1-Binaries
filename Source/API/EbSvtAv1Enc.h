﻿/*
* Copyright(c) 2019 Intel Corporation
*
* This source code is subject to the terms of the BSD 2 Clause License and
* the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
* was not distributed with this source code in the LICENSE file, you can
* obtain it at https://www.aomedia.org/license/software-license. If the Alliance for Open
* Media Patent License 1.0 was not distributed with this source code in the
* PATENTS file, you can obtain it at https://www.aomedia.org/license/patent-license.
*/

#ifndef EbSvtAv1Enc_h
#define EbSvtAv1Enc_h

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdint.h>
#include "EbSvtAv1.h"
#include <stdlib.h>
#include <stdio.h>

//***HME***
#define EB_HME_SEARCH_AREA_COLUMN_MAX_COUNT 2
#define EB_HME_SEARCH_AREA_ROW_MAX_COUNT 2
#define MAX_HIERARCHICAL_LEVEL 6
#define REF_LIST_MAX_DEPTH 4
#if TUNE_NEW_M11
#if TUNE_NEW_M12
#if FTR_M13
#define MAX_ENC_PRESET 13
#else
#define MAX_ENC_PRESET 12
#endif
#else
#define MAX_ENC_PRESET 11
#endif
#else
#define MAX_ENC_PRESET 10
#endif
#define NUM_MV_COMPONENTS 2
#define NUM_MV_HIST 2
#define MAX_MV_HIST_SIZE  2 * REF_LIST_MAX_DEPTH * NUM_MV_COMPONENTS * NUM_MV_HIST
#define DEFAULT -1

#define EB_BUFFERFLAG_EOS 0x00000001 // signals the last packet of the stream
#define EB_BUFFERFLAG_SHOW_EXT \
    0x00000002 // signals that the packet contains a show existing frame at the end
#define EB_BUFFERFLAG_HAS_TD 0x00000004 // signals that the packet contains a TD
#define EB_BUFFERFLAG_IS_ALT_REF 0x00000008 // signals that the packet contains an ALT_REF frame
#define EB_BUFFERFLAG_ERROR_MASK \
    0xFFFFFFF0 // mask for signalling error assuming top flags fit in 4 bits. To be changed, if more flags are added.

/************************************************
 * Prediction Structure Config Entry
 *   Contains the basic reference lists and
 *   configurations for each Prediction Structure
 *   Config Entry.
 ************************************************/
typedef struct PredictionStructureConfigEntry {
    uint32_t temporal_layer_index;
    uint32_t decode_order;
    int32_t  ref_list0[REF_LIST_MAX_DEPTH];
    int32_t  ref_list1[REF_LIST_MAX_DEPTH];
} PredictionStructureConfigEntry;

#if CLN_TF_SIG
typedef struct TfControls {
    // Filtering set
    uint8_t  enabled;                  // Specifies whether the current input will be filtered or not (0: OFF, 1: ON)
    uint8_t  do_chroma;                // Specifies whether the U& V planes will be filered or not (0: filter all planes, 1 : filter Y plane only)
    uint8_t  use_medium_filter;        // Specifies whether the weights generation will use approximations or not (0: do not use approximations, 1: per-block weights derivation, use an approximated exponential & log, use an approximated noise level)
    uint8_t  use_fast_filter;          // Specifies whether the weights derivation will use the distance factor(MV - based correction) and the 5x5 window error or not (0: OFF, 1 : ON)
    uint8_t  use_fixed_point;          // Specifies noise-level-estimation and filtering precision (0: use float/double precision, 1: use fixed point precision)

    // Number of reference frame(s) set
    uint8_t  num_past_pics;            // Specifies the default number of frame(s) from past
    uint8_t  num_future_pics;          // Specifies the default number of frame(s) from future
    uint8_t  noise_adjust_past_pics;   // Specifies whether num_past_pics will be incremented or not based on the noise level of the central frame(0: OFF or 1 : ON)
    uint8_t  noise_adjust_future_pics; // Specifies whether num_future_pics will be incremented or not based on the noise level of the central frame(0: OFF or 1 : ON)
    uint8_t  use_intra_for_noise_est;  // Specifies whether to use the key- rame noise level for all inputs or to re - compute the noise level for each input
    uint8_t  activity_adjust_th;       // Specifies whether num_past_picsand num_future_pics will be decremented or not based on the activity of the outer reference frame(s) compared to the central frame(∞: OFF, else remove the reference frame if the cumulative differences between the histogram bins of the central frameand the histogram bins of the reference frame is higher than activity_adjust_th
    uint8_t  max_num_past_pics;        // Specifies the maximum number of frame(s) from past(after all adjustments)
    uint8_t  max_num_future_pics;      // Specifies the maximum number of frame(s) from future(after all adjustments)

    // Motion search
    uint8_t  hme_me_level;             // Specifies the accuracy of the ME search (note that ME performs a HME search, then a Full - Pel search).
    uint8_t  half_pel_mode;            // Specifies the accuracy of the Half-Pel search (0: OFF, 1 : perform refinement for the 8 neighboring positions, 2/3 : perform refinement for the 2 horizontal-neighboring positions and for the 2 vertical-neighboring positions, but not for all the 4 diagonal-neighboring positions = function(horizontal & vertical distortions)
    uint8_t  quarter_pel_mode;         // Specifies the accuracy of the Quarter-Pel search (0: OFF, 1 : perform refinement for the 8 neighboring positions, 2/3 : perform refinement for the 2 horizontal-neighboring positions and for the 2 vertical-neighboring positions, but not for all the 4 diagonal-neighboring positions = function(horizontal & vertical distortions)
    uint8_t  eight_pel_mode;           // Specifies the accuracy of the Eight-Pel search (0: OFF, 1 : perform refinement for the 8 neighboring positions)
    uint8_t  use_8bit_subpel;          // Specifies whether the Sub-Pel search for a 10bit input will be performed in 8bit resolution(0: OFF, 1 : ON, NA if 8bit input)
    uint8_t  avoid_2d_qpel;            // Specifies whether the Sub-Pel positions that require a 2D interpolation will be tested or not (0: OFF, 1 : ON, NA if 16x16 block or if the Sub-Pel mode is set to 1)
    uint8_t  use_2tap;                 // Specifies the Sub-Pel search filter type(0: regular, 1 : bilinear, NA if 16x16 block or if the Sub - Pel mode is set to 1)
    uint8_t  sub_sampling_shift;       // Specifies whether sub-sampled input / prediction will be used at the distortion computation of the Sub-Pel search
    uint64_t pred_error_32x32_th;      // Specifies the 32x32 prediction error(after subpel) under which the subpel for the 16x16 block(s) is bypassed
    uint32_t me_exit_th;               // Specifies whether to exit ME after HME or not (0: perform both HME and Full-Pel search, else if the HME distortion is less than me_exit_th then exit after HME(i.e. do not perform the Full-Pel search), NA if use_fast_filter is set 0)
    uint8_t  use_pred_64x64_only_th;   // Specifies whether to perform Sub-Pel search for only the 64x64 block or to use default size(s) (32x32 or/ and 16x16) (∞: perform Sub-Pel search for default size(s), else if the deviation between the 64x64 ME distortion and the sum of the 4 32x32 ME distortions is less than use_pred_64x64_only_th then perform Sub - Pel search for only the 64x64 block, NA if use_fast_filter is set 0)

} TfControls;
#else
typedef struct TfControls {
    uint8_t  enabled;
    uint8_t  num_past_pics;            // Number of frame(s) from past
    uint8_t  num_future_pics;          // Number of frame(s) from future
    uint8_t  noise_adjust_past_pics;   // 0: noise-based adjustment OFF | 1: up to 3 additional frame(s) from the past based on the noise level
    uint8_t  noise_adjust_future_pics; // 0: noise-based adjustment OFF | 1: up to 3 additional frame(s) from the future based on the noise level
    uint8_t  activity_adjust_th;       // The abs diff between the histogram of the central frame and the reference frame beyond which the reference frame is removed
    uint8_t  max_num_past_pics;        // Max number of frame(s) from past
    uint8_t  max_num_future_pics;      // Max number of frame(s) from future
    uint8_t  hme_me_level;             // HME/ME Search Level
    uint8_t  half_pel_mode;            // 0: do not perform half-pel refinement     | 1: perform half-pel refinement for the 8-positions    | 2: perform half-pel refinement for only 4-positions (H and V only)
    uint8_t  quarter_pel_mode;         // 0: do not perform quarter-pel refinement  | 1: perform quarter-pel refinement for the 8-positions | 2: perform half-pel refinement for only 4-positions (H and V only)
    uint8_t  eight_pel_mode;           // 0: do not perform eight-pel refinement    | 1: perform eight-pel refinement for the 8-positions   | 2: eight half-pel refinement for only 4-positions (H and V only)
    uint8_t  do_chroma;                // 0: do not filter chroma | 1: filter chroma
#if OPT_UPGRADE_TF
#if !FIX_TF_FILTER_64x64_PATH
    uint8_t  use_pred_64x64_only_th;   // 0: subpel/pred error for 32x32(s) or/and 16x16(s) | > 0: subpel/pred error for 64x64 only if 64x64-to-32x32 dist deviation less than use_pred_64x64_only_th
    uint32_t me_exit_th;               // early exit ME_TF if HME distortion < me_exit_th
#endif
#endif
    uint64_t pred_error_32x32_th;      // The 32x32 pred error (post-subpel) under which subpel for the 16x16 block(s) is bypassed
#if !CLN_TF_CTRLS
    int64_t  me_16x16_to_8x8_dev_th;   // The 16x16-to-8x8 me-distortion deviation beyond which the number of reference frames is capped to [max_64x64_past_pics, max_64x64_future_pics] @ the level of a 64x64 Block
    uint64_t max_64x64_past_pics;      // The max number of past reference frames if me_16x16_to_8x8_dev > me_16x16_to_8x8_dev_th
    uint64_t max_64x64_future_pics;    // The max number of future reference frames if me_16x16_to_8x8_dev > me_16x16_to_8x8_dev_th
#endif
#if OPT_TF
    uint8_t sub_sampling_shift;         // Use subsampled prediction and sse;
#endif
#if OPT_TFILTER
    uint8_t use_fast_filter;  //simple filter using fixed weight per block, no MV based correction, approximated exp
#endif
#if FIXED_POINTS_PLANEWISE
    uint8_t use_fixed_point;  //Enable calculations on fixed points instead of float/double
    uint8_t use_medium_filter;//simple filter using weight per quarter block
#endif
#if FIX_TF_FILTER_64x64_PATH
    uint8_t  use_pred_64x64_only_th;   // [valid for only use_fast_filter=1] 0: subpel/pred error for 32x32(s) or/and 16x16(s) | > 0: subpel/pred error for 64x64 only if 64x64-to-32x32 dist deviation less than use_pred_64x64_only_th
    uint32_t me_exit_th;               // [valid for only use_fast_filter=1] early exit ME_TF if HME distortion < me_exit_th
#endif
#if OPT_TFILTER
    uint8_t avoid_2d_qpel;    //avoid 2d qpel positions in 32x32 search
    uint8_t use_2tap;         //use bilinear sub pel filter in 32x32 search
#endif
#if OPT_NOISE_LEVEL
    uint8_t use_intra_for_noise_est; //use I frame for noise estimation
#endif
#if OPT_TF_8BIT_SUBPEL
    uint8_t use_8bit_subpel; // Perform TF subpel search in 8bit for 10bit content (this setting won't affect 8bit content)
#endif
} TfControls;
#endif
// super-res modes
typedef enum {
    SUPERRES_NONE, // No frame superres allowed.
    SUPERRES_FIXED, // All frames are coded at the specified scale, and super-resolved.
    SUPERRES_RANDOM, // All frames are coded at a random scale, and super-resolved.
    SUPERRES_QTHRESH, // Superres scale for a frame is determined based on q_index.
    SUPERRES_AUTO, // Automatically select superres for appropriate frames.
    SUPERRES_MODES
} SUPERRES_MODE;

typedef enum {
    SVT_AV1_STREAM_INFO_START = 1,

    // The output is SvtAv1FixedBuf*
    // Two use this, you need:
    // 1. set the EbSvtAv1EncConfiguration.rc_firstpass_stats_out to EB_TRUE
    // 2. call this when you got EB_BUFFERFLAG_EOS
    SVT_AV1_STREAM_INFO_FIRST_PASS_STATS_OUT = SVT_AV1_STREAM_INFO_START,

    SVT_AV1_STREAM_INFO_END,
} SVT_AV1_STREAM_INFO_ID;

/*!\brief Generic fixed size buffer structure
 *
 * This structure is able to hold a reference to any fixed size buffer.
 */
typedef struct SvtAv1FixedBuf {
    void *   buf; /**< Pointer to the data. Does NOT own the data! */
    uint64_t sz; /**< Length of the buffer, in chars */
} SvtAv1FixedBuf; /**< alias for struct aom_fixed_buf */

#if OPT_FIRST_PASS2
typedef struct IppPassControls {
    uint8_t skip_frame_first_pass; // Enable the ability to skip frame
    uint8_t ipp_ds; // use downsampled version in ipp pass
    uint8_t bypass_blk_step; // bypass every other row and col
    uint8_t dist_ds; // downsample distortion
    uint8_t bypass_zz_check; // Bypas the (0,0)_MV check against HME_MV before performing ME
    uint8_t use8blk;
    uint8_t reduce_me_search; //Reduce HME_ME SR areas
} IppPassControls;
#endif
#if CLN_MERGE_MRP_SIG
typedef struct MrpCtrls {
    // Referencing scheme
    uint8_t referencing_scheme; // 0 or 1

    // SC signals
    uint8_t sc_base_ref_list0_count;
    uint8_t sc_base_ref_list1_count;
    uint8_t sc_non_base_ref_list0_count;
    uint8_t sc_non_base_ref_list1_count;
    // non-SC signals
    uint8_t base_ref_list0_count;
    uint8_t base_ref_list1_count;
    uint8_t non_base_ref_list0_count;
    uint8_t non_base_ref_list1_count;
} MrpCtrls;
#endif
// Will contain the EbEncApi which will live in the EncHandle class
// Only modifiable during config-time.
typedef struct EbSvtAv1EncConfiguration {
    // Encoding preset

    /* A preset defining the quality vs density tradeoff point that the encoding
     * is to be performed at. 0 is the highest quality mode, 3 is the highest
     * density mode.
     *
     * Default is defined as MAX_ENC_PRESET. */
    int8_t enc_mode;

    // GOP Structure

    /* The intra period defines the interval of frames after which you insert an
     * Intra refresh. It is strongly recommended to set the value to multiple of
     * 8 minus 1 the closest to 1 second (e.g. 55, 47, 31, 23 should be used for
     * 60, 50, 30, (24 or 25) respectively.
     *
     * -1 = no intra update.
     * -2 = auto.
     *
     * Default is -2. */
    int32_t intra_period_length;
    /* Random access.
     *
     * 1 = CRA, open GOP.
     * 2 = IDR, closed GOP.
     *
     * Default is 1. */
    uint32_t intra_refresh_type;
    /* Number of hierarchical layers used to construct GOP.
     * Minigop size = 2^HierarchicalLevels.
     *
     * Default is 3. */
    uint32_t hierarchical_levels;

    /* Prediction structure used to construct GOP. There are two main structures
     * supported, which are: Low Delay (P or B) and Random Access.
     *
     * In Low Delay structure, pictures within a mini GOP refer to the previously
     * encoded pictures in display order. In other words, pictures with display
     * order N can only be referenced by pictures with display order greater than
     * N, and it can only refer pictures with picture order lower than N. The Low
     * Delay structure can be flat structured (e.g. IPPPPPPP...) or hierarchically
     * structured. B/b pictures can be used instead of P/p pictures. However, the
     * reference picture list 0 and the reference picture list 1 will contain the
     * same reference picture.
     *
     * In Random Access structure, the B/b pictures can refer to reference pictures
     * from both directions (past and future).
     *
     * Default is 2. */
    uint8_t pred_structure;

    // Input Info
    /* The width of input source in units of picture luma pixels.
     *
     * Default is 0. */
    uint32_t source_width;
    /* The height of input source in units of picture luma pixels.
     *
     * Default is 0. */
    uint32_t source_height;

    /* The frequecy of images being displayed. If the number is less than 1000,
     * the input frame rate is an integer number between 1 and 60, else the input
     * number is in Q16 format, shifted by 16 bits, where max allowed is 240 fps.
     * If FrameRateNumerator and FrameRateDenominator are both not equal to zero,
     * the encoder will ignore this parameter.
     *
     * Default is 25. */
    uint32_t frame_rate;

    /* Frame rate numerator. When zero, the encoder will use -fps if
     * FrameRateDenominator is also zero, otherwise an error is returned.
     *
     * Default is 0. */
    uint32_t frame_rate_numerator;
    /* Frame rate denominator. When zero, the encoder will use -fps if
     * FrameRateNumerator is also zero, otherwise an error is returned.
     *
     * Default is 0. */
    uint32_t frame_rate_denominator;
    /* Specifies the bit depth of input video.
     *
     * 8 = 8 bit.
     * 10 = 10 bit.
     *
     * Default is 8. */
    uint32_t encoder_bit_depth;
    /* Specifies whether to use 16bit pipeline.
     *
     * 0: 8 bit pipeline.
     * 1: 16 bit pipeline.
     * Now 16bit pipeline is only enabled in filter
     * Default is 0. */
    EbBool is_16bit_pipeline;
    /* Specifies the chroma subsampleing format of input video.
     *
     * 0 = mono.
     * 1 = 420.
     * 2 = 422.
     * 3 = 444.
     *
     * Default is 1. */
    EbColorFormat encoder_color_format;
    /* Offline packing of the 2bits: requires two bits packed input.
     *
     * Default is 0. */
    uint32_t compressed_ten_bit_format;

    /* Super block size for motion estimation
    *
    * Default is 64. */
    uint32_t sb_sz;

    /* Super block size (mm-signal)
    *
    * Default is 128. */
    uint32_t super_block_size;
    /* The maximum partitioning depth with 0 being the superblock depth
    *
    * Default is 4. */
    uint32_t partition_depth;

    /* Instruct the library to calculate the recon to source for PSNR calculation
    *
    * Default is 0.*/
    uint32_t stat_report;

    // Quantization
    /* Initial quantization parameter for the Intra pictures used under constant
     * qp rate control mode.
     *
     * Default is 50. */
    uint32_t qp;

    /* force qp values for every picture that are passed in the header pointer
    *
    * Default is 0.*/
    EbBool use_qp_file;

    /* use fixed qp offset for every picture based on temporal layer index
    *
    * Default is 0.*/
    EbBool use_fixed_qindex_offsets;
    int32_t qindex_offsets[EB_MAX_TEMPORAL_LAYERS];
    int32_t key_frame_chroma_qindex_offset;
    int32_t key_frame_qindex_offset;
    int32_t chroma_qindex_offsets[EB_MAX_TEMPORAL_LAYERS];
    /* input buffer for the second pass */
    SvtAv1FixedBuf rc_twopass_stats_in;
    /* generate first pass stats output.
    * when you set this to EB_TRUE, and you got the EB_BUFFERFLAG_EOS,
    * you can get the encoder stats using:
    *
    * SvtAv1FixedBuf first_pass_stat;
    * EbErrorType ret = svt_av1_enc_get_stream_info(component_handle,
    *     SVT_AV1_STREAM_INFO_FIRST_PASS_STATS_OUT, &first_pass_stat);
    *
    * Default is 0.*/
    EbBool rc_firstpass_stats_out;
#if FTR_MULTI_PASS_API
    EbBool rc_middlepass_stats_out;
    uint8_t    passes;
#if TUNE_MULTI_PASS
    MultiPassModes multi_pass_mode;
#endif
#if FTR_OPT_MPASS_DOWN_SAMPLE
    EbBool rc_middlepass_ds_stats_out;
#endif
#endif
    /* Enable picture QP scaling between hierarchical levels
    *
    * Default is null.*/
    uint32_t enable_qp_scaling_flag;

    // Deblock Filter
    /* Flag to disable the Deblocking Loop Filtering.
     *
     * Default is 0. */
    EbBool disable_dlf_flag;

    /* Film grain denoising the input picture
    * Flag to enable the denoising
    *
    * Default is 0. */
    uint32_t film_grain_denoise_strength;

    /* Warped motion
    *
    * Default is -1. */
    int enable_warped_motion;

    /* Global motion
    *
    * Default is 1. */
    EbBool enable_global_motion;

    /* CDEF Level
    *
    * Default is -1. */
    int cdef_level;

    /* Restoration filtering
    *  enable/disable
    *  set Self-Guided (sg) mode
    *  set Wiener (wn) mode
    *
    * Default is -1. */
    int enable_restoration_filtering;
    int sg_filter_mode;
    int wn_filter_mode;

    /* enable angle intra
    *
    * Default is -1. */
    int intra_angle_delta;

    /* inter intra compound
    *
    * Default is -1. */
    int inter_intra_compound;

    /* enable paeth
    *
    * Default is -1. */
    int enable_paeth;

    /* mrp level
    *
    * Default is -1. */
    int mrp_level;

    /* enable smooth
    *
    * Default is -1. */
    int enable_smooth;
    /* motion field motion vector
    *
    *  Default is -1. */
    int enable_mfmv;
    /* redundant block
    *
    * Default is -1. */
    int enable_redundant_blk;
#if FIX_REMOVE_PD1
    /* spatial sse in full loop
    *
    * -1: Default, 0: OFF, 1: ON. */
    int spatial_sse_full_loop_level;
#else
    /* spatial sse in full loop
    *
    * -1: Default, 0: OFF in PD_PASS_2, 1: Fully ON in PD_PASS_2. */
    int spatial_sse_full_loop_level;
#endif
    /* over boundry block
    *
    * Default is -1. */
    int over_bndry_blk;
    /* new nearest comb injection
    *
    * Default is -1. */
    int new_nearest_comb_inject;

    /* nsq table
    *
    * Default is -1. */
    int nsq_table;
    /* frame end cdf update
    *
    * Default is -1. */
    int frame_end_cdf_update;

    /* Predictive Me
    *
    * Default is -1. */
    int pred_me;

    /* Bipred 3x3 Injection
    *
    * Default is -1. */
    int bipred_3x3_inject;

    /* Compound Mode
    *
    * Default is -1. */
    int compound_level;

    /* Chroma mode
    *
    * Level                Settings
    * CHROMA_MODE_0  0     Full chroma search @ MD
    * CHROMA_MODE_1  1     Fast chroma search @ MD
    * CHROMA_MODE_2  2     Chroma blind @ MD + CFL @ EP
    * CHROMA_MODE_3  3     Chroma blind @ MD + no CFL @ EP
    *
    * Default is -1 (AUTO) */
    int set_chroma_mode;

    /* Disable chroma from luma (CFL)
     *
     * Default is -1 (auto) */
    int disable_cfl_flag;
#if FIX_REMOVE_PD1
        /* obmc_level specifies the level of the OBMC feature that would be
     * considered when the level is specified in the command line instruction (CLI).
     * The meaning of the feature level in the CLI is different from that for
     * the default settings. See description of pic_obmc_level for the full details.
     *
     * The table below specifies the meaning of obmc_level when specified in the CLI.
     *     obmc_level   | Command Line Settings
     *        -1        | Default settings (auto)
     *         0        | OFF everywhere in encoder
     *         1        | ON
     *
     * Default is -1 (auto). */
#else
    /* obmc_level specifies the level of the OBMC feature that would be
     * considered when the level is specified in the command line instruction (CLI).
     * The meaning of the feature level in the CLI is different from that for
     * the default settings. See description of pic_obmc_level for the full details.
     *
     * The table below specifies the meaning of obmc_level when specified in the CLI.
     *     obmc_level   | Command Line Settings
     *        -1        | Default settings (auto)
     *         0        | OFF everywhere in encoder
     *         1        | Fully ON in PD_PASS_2
     *         2        | Level 2 everywhere in PD_PASS_2
     *         3        | Level 3 everywhere in PD_PASS_3
     *
     * Default is -1 (auto). */
#endif
    int8_t obmc_level;
#if FIX_REMOVE_PD1
    /* RDOQ
    *
    * -1: Default, 0: OFF, 1: ON. */
#else
    /* RDOQ
    *
    * -1: Default, 0: OFF in PD_PASS_2, 1: Fully ON in PD_PASS_2. */
#endif
    int rdoq_level;
#if FIX_REMOVE_PD1
    /* Filter intra prediction
    *
    * The table below specifies the meaning of filter_intra_level when specified in the CLI.
    * filter_intra_level | Command Line Settings
    *        -1          | Default settings (auto)
    *         0          | OFF everywhere in encoder
    *         1          | ON */
    int8_t filter_intra_level;
#else
    /* Filter intra prediction
    *
    * The table below specifies the meaning of filter_intra_level when specified in the CLI.
    * filter_intra_level | Command Line Settings
    *        -1          | Default settings (auto)
    *         0          | OFF everywhere in encoder
    *         1          | Fully ON in PD_PASS_2, Default settings in PD_PASS_0 */
    int8_t filter_intra_level;
#endif
    /* Intra Edge Filter
    *
    * Default is -1. */
    int enable_intra_edge_filter;

    /* Picture based rate estimation
    *
    * Default is - 1. */
    int pic_based_rate_est;

    /* Flag to enable the use of default ME HME parameters.
    *
    * Default is 1. */
    EbBool use_default_me_hme;

    /* Flag to enable Hierarchical Motion Estimation.
    *
    * Default is 1. */
    EbBool enable_hme_flag;

    /* Flag to enable the use of non-swaure partitions
    *
    * Default is 1. */
    EbBool ext_block_flag;
    // ME Parameters
    /* Number of search positions in the horizontal direction.
     *
     * Default depends on input resolution. */
    uint32_t search_area_width;
    /* Number of search positions in the vertical direction.
     *
     * Default depends on input resolution. */
    uint32_t search_area_height;

    // MD Parameters
    /* Enable the use of HBD (10-bit) for 10 bit content at the mode decision step
     *
     * 0 = 8bit mode decision
     * 1 = 10bit mode decision
     * 2 = Auto: 8bit & 10bit mode decision
     *
    * Default is 1. */
    int8_t enable_hbd_mode_decision;
#if FIX_REMOVE_PD1
    /* Palette Mode
    *
    * -1: Default, 0: OFF, 1: Fully ON, 2 ... 6: Faster levels */
    int32_t palette_level;
#else
    /* Palette Mode
    *
    * -1: Default, 0: OFF, 1: Fully ON, 2 ... 6: Faster levels
    * Levels 0 - 6 apply only to PD_PASS_2 */
    int32_t palette_level;
#endif
    // Rate Control

    /* Rate control mode.
     *
     * 0 = Constant QP.
     * 1 = Variable Bit Rate, achieve the target bitrate at entire stream.
     * 2 = Constrained Variable Bit Rate, achieve the target bitrate at each gop
     * Default is 0. */
    uint32_t rate_control_mode;
    /* Flag to enable the scene change detection algorithm.
     *
     * Default is 1. */
    uint32_t scene_change_detection;

    /* When RateControlMode is set to 1 it's best to set this parameter to be
     * equal to the Intra period value (such is the default set by the encoder).
     * When CQP is chosen, then a (2 * minigopsize +1) look ahead is recommended.
     *
     * Default depends on rate control mode.*/
    uint32_t look_ahead_distance;

    /* Enable TPL in look ahead
     * 0 = disable TPL in look ahead
     * 1 = enable TPL in look ahead
     * Default is 0  */
    uint8_t enable_tpl_la;

    /* Target bitrate in bits/second, only apllicable when rate control mode is
     * set to 2 or 3.
     *
     * Default is 7000000. */
    uint32_t target_bit_rate;
#if FTR_RC_CAP
    /* maximum bitrate in bits/second, only apllicable when rate control mode is
     * set to 0.
     *
     * Default is 0. */
    uint32_t max_bit_rate;
#endif
    /* VBV Buffer size */
    uint32_t vbv_bufsize;

    /* Maxium QP value allowed for rate control use, only applicable when rate
     * control mode is set to 1. It has to be greater or equal to minQpAllowed.
     *
     * Default is 63. */
    uint32_t max_qp_allowed;
    /* Minimum QP value allowed for rate control use, only applicable when rate
     * control mode is set to 1. It has to be smaller or equal to maxQpAllowed.
     *
     * Default is 0. */
    uint32_t min_qp_allowed;

    /* TWO PASS DATARATE CONTROL OPTIONS.
     * Indicates the bias (expressed on a scale of 0 to 100) for determining
     * target size for the current frame. The value 0 indicates the optimal CBR
     * mode value should be used, and 100 indicates the optimal VBR mode value
     * should be used. */
    uint32_t vbr_bias_pct;
    /* Indicates the minimum bitrate to be used for a single GOP as a percentage
     * of the target bitrate. */
    uint32_t vbr_min_section_pct;
    /* Indicates the maximum bitrate to be used for a single GOP as a percentage
     * of the target bitrate. */
    uint32_t vbr_max_section_pct;
    /* under_shoot_pct indicates the tolerance of the VBR algorithm to undershoot
     * and is used as a trigger threshold for more agressive adaptation of Q. Its
     * value can range from 0-100. */
    uint32_t under_shoot_pct;
    /* over_shoot_pct indicates the tolerance of the VBR algorithm to overshoot
     * and is used as a trigger threshold for more agressive adaptation of Q. Its
     * value can range from 0-1000. */
    uint32_t over_shoot_pct;
#if TUNE_CAP_CRF_OVERSHOOT
    /* over_shoot_pct indicates the tolerance of the Capped CRF algorithm to overshoot
     * and is used as a trigger threshold for more agressive adaptation of Q. Its
     * value can range from 0-1000. */
    uint32_t mbr_over_shoot_pct;
#endif
#if FTR_2PASS_CBR || FTR_1PASS_CBR
    /* Indicates the amount of data that will be buffered by the decoding
     * application prior to beginning playback, and is expressed in units of
     * time(milliseconds). */
    int64_t starting_buffer_level_ms;
    /* Indicates the amount of data that the encoder should try to maintain in the
     * decoder's buffer, and is expressed in units of time(milliseconds). */
    int64_t optimal_buffer_level_ms;
    /* Indicates the maximum amount of data that may be buffered by the decoding
     * application, and is expressed in units of time(milliseconds).*/
    int64_t maximum_buffer_size_ms;
#endif

    /* recode_loop indicates the recode levels,
     * DISALLOW_RECODE = 0, No recode.
     * ALLOW_RECODE_KFMAXBW = 1, Allow recode for KF and exceeding maximum frame bandwidth.
     * ALLOW_RECODE_KFARFGF = 2, Allow recode only for KF/ARF/GF frames.
     * ALLOW_RECODE = 3, Allow recode for all frames based on bitrate constraints.
     * ALLOW_RECODE_DEFAULT = 4, Default setting, ALLOW_RECODE_KFARFGF for M0~5 and
     *                                            ALLOW_RECODE_KFMAXBW for M6~8.
     * default is 4
     */
    uint32_t recode_loop;

    /* Flag to signal the content being a screen sharing content type
    *
    * Default is 0. */
    uint32_t screen_content_mode;

    /* Flag to control intraBC mode
    *  0      OFF
    *  1      slow
    *  2      faster
    *  3      fastest
    *
    * Default is -1 (DEFAULT behavior). */
    int intrabc_mode;

    /* Enable adaptive quantization within a frame using segmentation.
     *
     * Default is 2. */
    EbBool enable_adaptive_quantization;

    // Tresholds
    /* Flag to signal that the input yuv is HDR10 BT2020 using SMPTE ST2048, requires
     *
     * Default is 0. */
    uint32_t high_dynamic_range_input;

    /* Defined set of coding tools to create bitstream.
     *
     * 1 = Main, allows bit depth of 8.
     * 2 = Main 10, allows bit depth of 8 to 10.
     *
     * Default is 2. */
    uint32_t profile;
    /* Constraints for bitstream in terms of max bitrate and max buffer size.
     *
     * 0 = Main, for most applications.
     * 1 = High, for demanding applications.
     *
     * Default is 0. */
    uint32_t tier;
    /* Constraints for bitstream in terms of max bitrate and max buffer size.
     *
     * 0 = auto determination.
     *
     * Default is 0. */
    uint32_t level;

    /* CPU FLAGS to limit assembly instruction set used by encoder.
    * Default is CPU_FLAGS_ALL. */
    CPU_FLAGS use_cpu_flags;

    // Application Specific parameters

    /* ID assigned to each channel when multiple instances are running within the
     * same application. */
    uint32_t channel_id;
    uint32_t active_channel_count;

    /* Flag to enable the Speed Control functionality to achieve the real-time
    * encoding speed defined by dynamically changing the encoding preset to meet
    * the average speed defined in injectorFrameRate. When this parameter is set
    * to 1 it forces -inj to be 1 -inj-frm-rt to be set to the -fps.
    *
    * Default is 0. */
    uint32_t speed_control_flag;

    /* Flag to constrain motion vectors.
     *
     * 1: Motion vectors are allowed to point outside frame boundary.
     * 0: Motion vectors are NOT allowed to point outside frame boundary.
     *
     * Default is 1. */
    uint8_t unrestricted_motion_vector;

    // Threads management

    /* The number of logical processor which encoder threads run on. If
     * LogicalProcessorNumber and TargetSocket are not set, threads are managed by
     * OS thread scheduler. */
    uint32_t logical_processors;

    /* Unpin the execution .This option does not
    * set the execution to be pinned to a specific number of cores when set to 1. this allows the execution
    * of multiple encodes on the CPU without having to pin them to a specific mask
    * 1: unpinned
    * 0: pinned
    * default 1 */
    uint32_t unpin;

    /* Target socket to run on. For dual socket systems, this can specify which
     * socket the encoder runs on.
     *
     * -1 = Both Sockets.
     *  0 = Socket 0.
     *  1 = Socket 1.
     *
     * Default is -1. */
    int32_t target_socket;

    // Debug tools

    /* Output reconstructed yuv used for debug purposes. The value is set through
     * ReconFile token (-o) and using the feature will affect the speed of encoder.
     *
     * Default is 0. */
    uint32_t recon_enabled;
    /* Log 2 Tile Rows and colums . 0 means no tiling,1 means that we split the dimension
        * into 2
        * Default is 0. */
    int32_t tile_columns;
    int32_t tile_rows;

    /* To be deprecated.
 * Encoder configuration parameters below this line are to be deprecated. */

    /* Flag to enable Hierarchical Motion Estimation 1/16th of the picture
    *
    * Default is 1. */
    EbBool enable_hme_level0_flag;

    /* Flag to enable Hierarchical Motion Estimation 1/4th of the picture
    *
    * Default is 1. */
    EbBool enable_hme_level1_flag;

    /* Flag to enable Hierarchical Motion Estimation full sample of the picture
    *
    * Default is 1. */
    EbBool enable_hme_level2_flag;

    // HME Parameters
    /* Number of search positions in width and height for the HME
    *
    * Default depends on input resolution. */
    uint32_t number_hme_search_region_in_width;
    uint32_t number_hme_search_region_in_height;
    uint32_t hme_level0_total_search_area_width;
    uint32_t hme_level0_total_search_area_height;
    uint32_t hme_level0_search_area_in_width_array[EB_HME_SEARCH_AREA_COLUMN_MAX_COUNT];
    uint32_t hme_level0_search_area_in_height_array[EB_HME_SEARCH_AREA_ROW_MAX_COUNT];
    uint32_t hme_level1_search_area_in_width_array[EB_HME_SEARCH_AREA_COLUMN_MAX_COUNT];
    uint32_t hme_level1_search_area_in_height_array[EB_HME_SEARCH_AREA_ROW_MAX_COUNT];
    uint32_t hme_level2_search_area_in_width_array[EB_HME_SEARCH_AREA_COLUMN_MAX_COUNT];
    uint32_t hme_level2_search_area_in_height_array[EB_HME_SEARCH_AREA_ROW_MAX_COUNT];

    uint32_t ten_bit_format;

    /* Variables to control the use of ALT-REF (temporally filtered frames)
    */
    // -1: Default; 0: OFF; 1: ON
    int8_t  tf_level;
    EbBool  enable_overlays;
    TfControls tf_params_per_type[3]; // [I_SLICE][BASE][L1]

    // super-resolution parameters
    uint8_t superres_mode;
    uint8_t superres_denom;
    uint8_t superres_kf_denom;
    uint8_t superres_qthres;

    /* Prediction Structure user defined
   */
    PredictionStructureConfigEntry pred_struct[1 << (MAX_HIERARCHICAL_LEVEL - 1)];
    /* Flag to enable use prediction structure user defined
   *
   * Default is false. */
    EbBool enable_manual_pred_struct;
    /* The minigop size of prediction structure user defined
   *
   * Default is 0. */
    int32_t manual_pred_struct_entry_num;

    // Color description
    /* Color description present flag
    *
    * It is not necessary to set this parameter manually.
    * It is set internally to true once one of the color_primaries, transfer_characteristics or
    * matrix coefficients is set to non-default value.
    *
    Default is false. */
    EbBool color_description_present_flag;
    /* Color primaries
    *
    Default is 2 (CP_UNSPECIFIED). */
    uint8_t color_primaries;
    /* Transfer characteristics
    *
    Default is 2 (TC_UNSPECIFIED). */
    uint8_t transfer_characteristics;
    /* Matrix coefficients
    *
    Default is 2 (MC_UNSPECIFIED). */
    uint8_t matrix_coefficients;
    /* Color range
    *
    * 0: studio swing.
    * 1: full swing.
    Default is 0. */
    uint8_t color_range;
#if FIX_DATA_RACE_2PASS
    uint8_t enable_adaptive_mini_gop;
    uint8_t max_heirachical_level;
#endif
#if OPT_FIRST_PASS
#if !FIX_VBR_IPP
    uint8_t final_pass_rc_mode;
#endif
#endif
#if OPT_FIRST_PASS2
    IppPassControls ipp_ctrls;
    uint8_t ipp_was_ds;
#endif
#if IPP_CTRL
    uint8_t final_pass_preset;
#endif
#if CLN_MERGE_MRP_SIG
    MrpCtrls mrp_ctrls;
#endif
} EbSvtAv1EncConfiguration;

/**
 * Returns a string containing "v$tag-$commit_count-g$hash${dirty:+-dirty}"
 * @param[out] SVT_AV1_CVS_VERSION
 */
EB_API const char *svt_av1_get_version(void);

/**
 * Prints the version header and build information to the file
 * specified by the SVT_LOG_FILE environment variable or stderr
 */
EB_API void svt_av1_print_version(void);

/* STEP 1: Call the library to construct a Component Handle.
     *
     * Parameter:
     * @ **p_handle      Handle to be called in the future for manipulating the
     *                  component.
     * @ *p_app_data      Callback data.
     * @ *config_ptr     Pointer passed back to the client during callbacks, it will be
     *                  loaded with default params from the library. */
EB_API EbErrorType svt_av1_enc_init_handle(
    EbComponentType **p_handle, void *p_app_data,
    EbSvtAv1EncConfiguration
        *config_ptr); // config_ptr will be loaded with default params from the library

/* STEP 2: Set all configuration parameters.
     *
     * Parameter:
     * @ *svt_enc_component              Encoder handler.
     * @ *pComponentParameterStructure  Encoder and buffer configurations will be copied to the library. */
EB_API EbErrorType svt_av1_enc_set_parameter(
    EbComponentType *svt_enc_component,
    EbSvtAv1EncConfiguration *
        pComponentParameterStructure); // pComponentParameterStructure contents will be copied to the library

/* STEP 3: Initialize encoder and allocates memory to necessary buffers.
     *
     * Parameter:
     * @ *svt_enc_component  Encoder handler. */
EB_API EbErrorType svt_av1_enc_init(EbComponentType *svt_enc_component);

/* OPTIONAL: Get stream headers at init time.
     *
     * Parameter:
     * @ *svt_enc_component   Encoder handler.
     * @ **output_stream_ptr  Output buffer. */
EB_API EbErrorType svt_av1_enc_stream_header(EbComponentType *    svt_enc_component,
                                             EbBufferHeaderType **output_stream_ptr);

/* OPTIONAL: Release stream headers at init time.
     *
     * Parameter:
     * @ *stream_header_ptr  stream header buffer. */
EB_API EbErrorType svt_av1_enc_stream_header_release(EbBufferHeaderType *stream_header_ptr);

/* STEP 4: Send the picture.
     *
     * Parameter:
     * @ *svt_enc_component  Encoder handler.
     * @ *p_buffer           Header pointer, picture buffer. */
#if OPT_FIRST_PASS2 && !FIX_DG
EB_API EbErrorType svt_av1_enc_send_picture(EbComponentType *   svt_enc_component,
    EbBufferHeaderType *p_buffer, int pass);
#else
EB_API EbErrorType svt_av1_enc_send_picture(EbComponentType *   svt_enc_component,
                                            EbBufferHeaderType *p_buffer);
#endif

/* STEP 5: Receive packet.
     * Parameter:
    * @ *svt_enc_component  Encoder handler.
     * @ **p_buffer          Header pointer to return packet with.
     * @ pic_send_done       Flag to signal that all input pictures have been sent, this call becomes locking one this signal is 1.
     * Non-locking call, returns EB_ErrorMax for an encode error, EB_NoErrorEmptyQueue when the library does not have any available packets.*/
EB_API EbErrorType svt_av1_enc_get_packet(EbComponentType *    svt_enc_component,
                                          EbBufferHeaderType **p_buffer, uint8_t pic_send_done);

/* STEP 5-1: Release output buffer back into the pool.
     *
     * Parameter:
     * @ **p_buffer          Header pointer that contains the output packet to be released. */
EB_API void svt_av1_enc_release_out_buffer(EbBufferHeaderType **p_buffer);

/* OPTIONAL: Fill buffer with reconstructed picture.
     *
     * Parameter:
     * @ *svt_enc_component  Encoder handler.
     * @ *p_buffer           Output buffer. */
EB_API EbErrorType svt_av1_get_recon(EbComponentType *   svt_enc_component,
                                     EbBufferHeaderType *p_buffer);

/* OPTIONAL: get stream information
     *
     * Parameter:
     * @ *svt_enc_component  Encoder handler.
     * @ *stream_info_id SVT_AV1_STREAM_INFO_ID.
     * @ *info         output, the type depends on id */
EB_API EbErrorType svt_av1_enc_get_stream_info(EbComponentType *svt_enc_component,
                                               uint32_t stream_info_id, void *info);

/* STEP 6: Deinitialize encoder library.
     *
     * Parameter:
     * @ *svt_enc_component  Encoder handler. */
EB_API EbErrorType svt_av1_enc_deinit(EbComponentType *svt_enc_component);

/* STEP 7: Deconstruct encoder handler.
     *
     * Parameter:
     * @ *svt_enc_component  Encoder handler. */
EB_API EbErrorType svt_av1_enc_deinit_handle(EbComponentType *svt_enc_component);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // EbSvtAv1Enc_h
