/*
* Copyright(c) 2019 Intel Corporation
*
* This source code is subject to the terms of the BSD 2 Clause License and
* the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
* was not distributed with this source code in the LICENSE file, you can
* obtain it at https://www.aomedia.org/license/software-license. If the Alliance for Open
* Media Patent License 1.0 was not distributed with this source code in the
* PATENTS file, you can obtain it at https://www.aomedia.org/license/patent-license.
*/

#ifndef EbPictureControlSet_h
#define EbPictureControlSet_h

#include "EbSvtAv1Enc.h"
#include "EbDefinitions.h"
#include "EbSystemResourceManager.h"
#include "EbPictureBufferDesc.h"
#include "EbCodingUnit.h"
#include "EbEntropyCodingObject.h"
#include "EbDefinitions.h"
#include "EbPredictionStructure.h"
#include "EbNeighborArrays.h"
#include "EbModeDecisionSegments.h"
#include "EbEncDecSegments.h"
#include "EbRestoration.h"
#include "EbObject.h"
#include "noise_model.h"
#include "EbSegmentationParams.h"
#include "EbAv1Structs.h"
#include "EbMdRateEstimation.h"
#include "EbEncCdef.h"
#include "Av1Common.h"

#include "av1me.h"
#include "hash_motion.h"
#include "firstpass.h"

#ifdef __cplusplus
extern "C" {
#endif
#define HISTOGRAM_NUMBER_OF_BINS 256
#define MAX_NUMBER_OF_REGIONS_IN_WIDTH 4
#define MAX_NUMBER_OF_REGIONS_IN_HEIGHT 4
#define MAX_REF_QP_NUM 81
#define QPS_SW_THRESH 8 // 100 to shut QPS/QPM (i.e. CORE only)
// BDP OFF
#define MD_NEIGHBOR_ARRAY_INDEX 0
#define MULTI_STAGE_PD_NEIGHBOR_ARRAY_INDEX 4
#define NEIGHBOR_ARRAY_TOTAL_COUNT 5
#define AOM_QM_BITS 5

typedef struct DepCntPicInfo {
    uint64_t pic_num;
    int32_t
        dep_cnt_diff; //increase(e.g 4L->5L) or decrease of dep cnt . not including the run-time decrease
} DepCntPicInfo;
typedef struct EbDownScaledBufDescPtrArray {
    EbPictureBufferDesc *picture_ptr;
    EbPictureBufferDesc *quarter_picture_ptr;
    EbPictureBufferDesc *sixteenth_picture_ptr;
    uint64_t             picture_number;
} EbDownScaledBufDescPtrArray;

typedef struct EbDownScaledObject {
    EbDctor dctor;
    //EbPictureBufferDesc *picture_ptr; // original picture, just a pointer, don't allocate resource here
    EbPictureBufferDesc *quarter_picture_ptr;
    EbPictureBufferDesc *sixteenth_picture_ptr;
    //uint64_t            picture_number;
} EbDownScaledObject;

typedef struct EbDownScaledObjectDescInitData {
    EbPictureBufferDescInitData quarter_picture_desc_init_data;
    EbPictureBufferDescInitData sixteenth_picture_desc_init_data;

    // whether enable 1/4,1/16 8bit luma for in_loop global motion
    uint8_t enable_quarter_luma_input;
    uint8_t enable_sixteenth_luma_input;
} EbDownScaledObjectDescInitData;
typedef struct MacroblockPlane {
    // Quantizer setings
    // These are used/accessed only in the quantization process
    // RDO does not / must not depend on any of these values
    // All values below share the coefficient scale/shift used in TX
    const int16_t *quant_fp_qtx;
    const int16_t *round_fp_qtx;
    const int16_t *quant_qtx;
    const int16_t *quant_shift_qtx;
    const int16_t *zbin_qtx;
    const int16_t *round_qtx;
    const int16_t *dequant_qtx;
} MacroblockPlane;
// The Quants structure is used only for internal quantizer setup in
// av1_quantize.c.
// All of its fields use the same coefficient shift/scaling at TX.
typedef struct Quants {
    // 0: dc 1: ac 2-8: ac repeated to SIMD width
    DECLARE_ALIGNED(16, int16_t, y_quant[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, y_quant_shift[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, y_zbin[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, y_round[QINDEX_RANGE][8]);

    // if we want to deprecate the current use of y_quant.
    DECLARE_ALIGNED(16, int16_t, y_quant_fp[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, u_quant_fp[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, v_quant_fp[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, y_round_fp[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, u_round_fp[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, v_round_fp[QINDEX_RANGE][8]);

    DECLARE_ALIGNED(16, int16_t, u_quant[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, v_quant[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, u_quant_shift[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, v_quant_shift[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, u_zbin[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, v_zbin[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, u_round[QINDEX_RANGE][8]);
    DECLARE_ALIGNED(16, int16_t, v_round[QINDEX_RANGE][8]);
} Quants;

// The Dequants structure is used only for internal quantizer setup in
// av1_quantize.c.
// Fields are sufffixed according to whether or not they're expressed in
// the same coefficient shift/precision as TX or a fixed Q3 format.
typedef struct Dequants {
    DECLARE_ALIGNED(16, int16_t,
                    y_dequant_qtx[QINDEX_RANGE][8]); // 8: SIMD width
    DECLARE_ALIGNED(16, int16_t,
                    u_dequant_qtx[QINDEX_RANGE][8]); // 8: SIMD width
    DECLARE_ALIGNED(16, int16_t,
                    v_dequant_qtx[QINDEX_RANGE][8]); // 8: SIMD width
    DECLARE_ALIGNED(16, int16_t, y_dequant_q3[QINDEX_RANGE][8]); // 8: SIMD width
    DECLARE_ALIGNED(16, int16_t, u_dequant_q3[QINDEX_RANGE][8]); // 8: SIMD width
    DECLARE_ALIGNED(16, int16_t, v_dequant_q3[QINDEX_RANGE][8]); // 8: SIMD width
} Dequants;

typedef struct MacroblockdPlane {
    //TranLow *dqcoeff;
    PlaneType    plane_type;
    int32_t      subsampling_x;
    int32_t      subsampling_y;
    struct Buf2D dst;
    int32_t      is_16bit;
    //struct Buf2D pre[2];
    //EntropyContext *above_context;
    //EntropyContext *left_context;
    // The dequantizers below are true dequntizers used only in the
    // dequantization process.  They have the same coefficient
    // shift/scale as TX.
    //int16_t seg_dequant_qtx[MAX_SEGMENTS][2];
    //uint8_t *color_index_map;
    // block size in pixels
    //uint8_t width, height;
    //QmVal *seg_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
    //QmVal *seg_qmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
    // the 'dequantizers' below are not literal dequantizer values.
    // They're used by encoder RDO to generate ad-hoc lambda values.
    // They use a hardwired Q3 coeff shift and do not necessarily match
    // the TX scale in use.
    //const int16_t *dequant_Q3;
} MacroblockdPlane;

struct PredictionUnit;

/**************************************
     * Segment-based Control Sets
     **************************************/

typedef struct EbMdcLeafData {
    uint32_t mds_idx;
    uint32_t tot_d1_blocks; //how many d1 bloks every parent square would have
} EbMdcLeafData;

typedef struct MdcSbData {
    uint32_t      leaf_count;
    EbMdcLeafData leaf_data_array[BLOCK_MAX_COUNT_SB_128];
    EbBool   split_flag[BLOCK_MAX_COUNT_SB_128];
    uint8_t  consider_block[BLOCK_MAX_COUNT_SB_128];
    uint8_t  refined_split_flag[BLOCK_MAX_COUNT_SB_128];
} MdcSbData;

/**************************************
     * MD Segment Control
     **************************************/
typedef struct MdSegmentCtrl {
    uint64_t completion_mask;
    EbHandle write_lock_mutex;
    uint32_t total_count;
    uint32_t column_count;
    uint32_t row_count;
    EbBool   in_progress;
    uint32_t current_row_idx;
} MdSegmentCtrl;

/**************************************
     * Picture Control Set
     **************************************/
struct CodedTreeblock_s;
struct SuperBlock;
#define MAX_MESH_STEP 4

typedef struct MeshPattern {
    int range;
    int interval;
} MeshPattern;

typedef struct CdfControls {
    uint8_t enabled; //1 if mv, or se, or coeff is ON
    uint8_t update_mv; //cdf update for mv
    uint8_t update_se; //cdf update for various syntax elements
    uint8_t update_coef; //cdf update for coeffs
} CdfControls;
typedef struct SpeedFeatures {
    // This allows us to use motion search at other sizes as a starting
    // point for this motion search and limits the search range around it.
    int adaptive_motion_search;

    // Flag for allowing some use of exhaustive searches;
    int allow_exhaustive_searches;

    // Threshold for allowing exhaistive motion search.
    int exhaustive_searches_thresh;

    // Maximum number of exhaustive searches for a frame.
    int max_exaustive_pct;

    // Pattern to be used for any exhaustive mesh searches.
    MeshPattern mesh_patterns[MAX_MESH_STEP];

} SpeedFeatures;
typedef struct EncDecSet {
    EbDctor          dctor;
    EbPictureBufferDesc *recon_picture_ptr;
    EbPictureBufferDesc *recon_picture16bit_ptr;
    EbPictureBufferDesc **quantized_coeff;
    EbObjectWrapper *enc_dec_wrapper_ptr;
    struct PictureParentControlSet *parent_pcs_ptr; //The parent of this PCS.
    EbObjectWrapper *               picture_parent_control_set_wrapper_ptr;

    uint16_t sb_total_count_unscaled;

} EncDecSet;
#if SS_OPT_CDEF_APPL
typedef struct CdefDirData {
    uint8_t dir[CDEF_NBLOCKS][CDEF_NBLOCKS];
    int32_t var[CDEF_NBLOCKS][CDEF_NBLOCKS];
} CdefDirData;
#endif
typedef struct PictureControlSet {
    /*!< Pointer to the dtor of the struct*/
    EbDctor          dctor;
    EbObjectWrapper *scs_wrapper_ptr;
    EbPictureBufferDesc *film_grain_picture_ptr;
    EbPictureBufferDesc *film_grain_picture16bit_ptr;
    EbPictureBufferDesc *input_frame16bit;

    struct PictureParentControlSet *parent_pcs_ptr; //The parent of this PCS.
    EbObjectWrapper *               picture_parent_control_set_wrapper_ptr;
    // Packetization (used to encode SPS, PPS, etc)
    Bitstream *bitstream_ptr;

    EbObjectWrapper *c_pcs_wrapper_ptr;

    // Reference Lists
    // Reference Lists
    EbObjectWrapper *ref_pic_ptr_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
    //EB_S64                                refPicPocArray[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];

    uint8_t  ref_pic_qp_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
    EB_SLICE ref_slice_type_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
#if !FTR_NEW_MULTI_PASS
    uint64_t ref_pic_referenced_area_avg_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
#endif
    double   ref_pic_r0[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
    // GOP
    uint64_t         picture_number;
    uint8_t          temporal_layer_index;
    EbColorFormat    color_format;
    EncDecSegments **enc_dec_segment_ctrl;
    uint16_t         enc_dec_coded_sb_count;

    // Entropy Process Rows
    EntropyTileInfo **entropy_coding_info;
    EbHandle          entropy_coding_pic_mutex;
    EbBool            entropy_coding_pic_reset_flag;
    uint8_t           tile_size_bytes_minus_1;
    EbHandle          intra_mutex;
#if FTR_INTRA_DETECTOR
    uint32_t          intra_coded_area;
#endif
#if FTR_COEFF_DETECTOR
    uint64_t          skip_coded_area;
#endif
    uint32_t          tot_seg_searched_cdef;
    EbHandle          cdef_search_mutex;

    uint16_t cdef_segments_total_count;
    uint8_t  cdef_segments_column_count;
    uint8_t  cdef_segments_row_count;

    uint64_t (*mse_seg[2])[TOTAL_STRENGTHS];
#if MS_CDEF_OPT1
    uint8_t *skip_cdef_seg;
#endif
#if SS_OPT_CDEF_APPL
    CdefDirData *cdef_dir_data;
#endif

    uint16_t *src[3]; //dlfed recon in 16bit form
    uint16_t *ref_coeff[3]; //input video in 16bit form

    uint32_t tot_seg_searched_rest;
    EbHandle rest_search_mutex;
    uint16_t rest_segments_total_count;
    uint8_t  rest_segments_column_count;
    uint8_t  rest_segments_row_count;

    // Slice Type
    EB_SLICE slice_type;

    // Rate Control
    uint8_t picture_qp;
    uint8_t dif_blk_delta_qp_depth;

    // SB Array
    uint16_t     sb_total_count;
    SuperBlock **sb_ptr_array;
#if FTR_VLPD1
    uint8_t* sb_intra;
    uint8_t* sb_skip;
    uint8_t* sb_64x64_mvp;
#endif
    // Mode Decision Neighbor Arrays
    NeighborArrayUnit **md_intra_luma_mode_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_skip_flag_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_mode_type_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_luma_recon_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_tx_depth_1_luma_recon_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_tx_depth_2_luma_recon_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_cb_recon_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_cr_recon_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];

    uint8_t             hbd_mode_decision;
    NeighborArrayUnit **md_luma_recon_neighbor_array16bit[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_tx_depth_1_luma_recon_neighbor_array16bit[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_tx_depth_2_luma_recon_neighbor_array16bit[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_cb_recon_neighbor_array16bit[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_cr_recon_neighbor_array16bit[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_luma_dc_sign_level_coeff_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit *
        *md_tx_depth_1_luma_dc_sign_level_coeff_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_cb_dc_sign_level_coeff_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_cr_dc_sign_level_coeff_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
    NeighborArrayUnit **md_txfm_context_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
#if FIX_SKIP_COEFF_CONTEXT
    NeighborArrayUnit **md_skip_coeff_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];
#endif
    NeighborArrayUnit **md_ref_frame_type_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];

    NeighborArrayUnit32 **md_interpolation_type_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];

    NeighborArrayUnit **mdleaf_partition_neighbor_array[NEIGHBOR_ARRAY_TOTAL_COUNT];

    // Encode Pass Neighbor Arrays
    NeighborArrayUnit **ep_intra_luma_mode_neighbor_array;
    NeighborArrayUnit **ep_intra_chroma_mode_neighbor_array;
    NeighborArrayUnit **ep_mv_neighbor_array;
    NeighborArrayUnit **ep_skip_flag_neighbor_array;
    NeighborArrayUnit **ep_mode_type_neighbor_array;
    NeighborArrayUnit **ep_luma_recon_neighbor_array;
    NeighborArrayUnit **ep_cb_recon_neighbor_array;
    NeighborArrayUnit **ep_cr_recon_neighbor_array;
    NeighborArrayUnit **ep_luma_recon_neighbor_array16bit;
    NeighborArrayUnit **ep_cb_recon_neighbor_array16bit;
    NeighborArrayUnit **ep_cr_recon_neighbor_array16bit;
    NeighborArrayUnit **ep_luma_dc_sign_level_coeff_neighbor_array;
    NeighborArrayUnit **ep_cr_dc_sign_level_coeff_neighbor_array;
    NeighborArrayUnit **ep_cb_dc_sign_level_coeff_neighbor_array;
#if REFCTR_SEP_ENCDEC
    NeighborArrayUnit **ep_luma_dc_sign_level_coeff_neighbor_array_update;
    NeighborArrayUnit **ep_cr_dc_sign_level_coeff_neighbor_array_update;
    NeighborArrayUnit **ep_cb_dc_sign_level_coeff_neighbor_array_update;
#endif
    NeighborArrayUnit **ep_partition_context_neighbor_array;

    // Entropy Coding Neighbor Arrays
    NeighborArrayUnit **mode_type_neighbor_array;
    NeighborArrayUnit **partition_context_neighbor_array;
    NeighborArrayUnit **intra_luma_mode_neighbor_array;
    NeighborArrayUnit **skip_flag_neighbor_array;
    NeighborArrayUnit **skip_coeff_neighbor_array;
    NeighborArrayUnit **
        luma_dc_sign_level_coeff_neighbor_array; // Stored per 4x4. 8 bit: lower 6 bits (COEFF_CONTEXT_BITS), shows if there is at least one Coef. Top 2 bit store the sign of DC as follow: 0->0,1->-1,2-> 1
    NeighborArrayUnit **
        cr_dc_sign_level_coeff_neighbor_array; // Stored per 4x4. 8 bit: lower 6 bits(COEFF_CONTEXT_BITS), shows if there is at least one Coef. Top 2 bit store the sign of DC as follow: 0->0,1->-1,2-> 1
    NeighborArrayUnit **
                          cb_dc_sign_level_coeff_neighbor_array; // Stored per 4x4. 8 bit: lower 6 bits(COEFF_CONTEXT_BITS), shows if there is at least one Coef. Top 2 bit store the sign of DC as follow: 0->0,1->-1,2-> 1
    NeighborArrayUnit **  txfm_context_array;
    NeighborArrayUnit **  ref_frame_type_neighbor_array;
    NeighborArrayUnit32 **interpolation_type_neighbor_array;

    NeighborArrayUnit **     segmentation_id_pred_array;
    SegmentationNeighborMap *segmentation_neighbor_map;

    ModeInfo **mi_grid_base; //2 SB Rows of mi Data are enough

    ModeInfo *mip;

    int32_t          mi_stride;
#if OPT_MI_MAP_MEMORY
    uint8_t disallow_4x4_all_frames; // true if 4x4 blocks are disallowed for all frames, and NSQ is disabled (since granularity is needed for 8x8 NSQ blocks).  Used to compute the offset for mip.
#endif
#if CLN_WM_SIG
    uint8_t wm_level;  //warped motion level
#endif
#if CLN_REG_PD_SIG_SET_0
    uint8_t cand_reduction_level;
    uint8_t txt_level;
    uint8_t tx_shortcut_level;
    uint8_t interpolation_search_level;
    uint8_t chroma_level;
    uint8_t cfl_level;
#endif
#if CLN_REG_PD_SIG_SET_1
    uint8_t new_nearest_near_comb_injection;
    uint8_t unipred3x3_injection;
    uint8_t bipred3x3_injection;
    uint8_t inter_compound_mode;
    uint8_t dist_based_ref_pruning;
    uint8_t spatial_sse_full_loop_level;
#endif
#if CLN_REG_PD_SIG_SET_2
    uint8_t parent_sq_coeff_area_based_cycles_reduction_level;
    uint32_t sq_weight;
    uint32_t max_part0_to_part1_dev;
    uint8_t md_inter_intra_level;
    uint8_t txs_level;
    uint8_t nic_level;
    uint8_t md_sq_mv_search_level;
    uint8_t md_nsq_mv_search_level;
    uint8_t md_pme_level;
    uint8_t mds0_level;
#endif
#if CLN_4X4_SIG
    uint8_t   pic_disallow_4x4; //disallow 4x4 at pic level
#endif
#if CLN_PD0_SIG
    EbBool pic_pd0_level; // pd0_level at pic level
#endif
#if CLN_SKIP_PD0_SIG
    uint8_t pic_skip_pd0; // skip_pd0 at pic level
#endif
#if CLN_DISALLOW_BELOW_16X16_SIG
    uint8_t pic_disallow_below_16x16; // disallow_below_16x16 signal at pic level
#endif
#if CLN_DEPTH_REMOVAL_SIG
    uint8_t pic_depth_removal_level; // depth_removal_level signal at the picture level
#endif
#if CLN_BLOCK_BASED_DEPTH_SIG
    uint8_t pic_block_based_depth_refinement_level; // block_based_depth_refinement_level signal set at the picture level
#endif
#if CLN_LPD1_LVL_SIG
    uint8_t pic_lpd1_lvl; // lpd1_lvl signal set at the picture level
#endif
#if CLN_BYP_ED_SIG
    EbBool    pic_bypass_encdec;
#endif
    EbReflist        colocated_pu_ref_list;
    EbEncMode        enc_mode;
    int32_t          cdef_preset[MAX_TILE_CNTS][4];
    WienerInfo       wiener_info[MAX_TILE_CNTS][MAX_MB_PLANE];
    SgrprojInfo      sgrproj_info[MAX_TILE_CNTS][MAX_MB_PLANE];
    SpeedFeatures    sf;
    SearchSiteConfig ss_cfg; //CHKN this might be a seq based
    HashTable        hash_table;
    CRC_CALCULATOR   crc_calculator1;
    CRC_CALCULATOR   crc_calculator2;

    FRAME_CONTEXT *                 ec_ctx_array;
    FRAME_CONTEXT                   md_frame_context;
    CdfControls                     cdf_ctrl;
    FRAME_CONTEXT                   ref_frame_context[REF_FRAMES];
    EbWarpedMotionParams            ref_global_motion[TOTAL_REFS_PER_FRAME];
    struct MdRateEstimationContext *md_rate_estimation_array;
    int8_t                          ref_frame_side[REF_FRAMES];
    TPL_MV_REF *                    tpl_mvs;
    uint8_t                         pic_filter_intra_level;
    TOKENEXTRA *                    tile_tok[64][64];
    //Put it here for deinit, don't need to go pcs->ppcs->av1_cm which may already be released
    uint16_t tile_row_count;
    uint16_t tile_column_count;
    uint16_t sb_total_count_pix;
    uint16_t sb_total_count_unscaled;
    // pointer to a scratch buffer used by self-guided restoration
    int32_t *rst_tmpbuf;

    EbPictureBufferDesc *temp_lf_recon_picture_ptr;
    EbPictureBufferDesc *temp_lf_recon_picture16bit_ptr;

    RestUnitSearchInfo *rusi_picture[3]; //for 3 planes

    RestorationInfo rst_info[MAX_MB_PLANE];
    // rst_end_stripe[i] is one more than the index of the bottom stripe
    // for tile row i.
    int32_t rst_end_stripe[MAX_TILE_ROWS];
#if TUNE_NEW_M11_2
#if CLN_REF_AREA
    uint8_t ref_intra_percentage;
    uint8_t ref_skip_percentage;
#else
    uint8_t intra_percentage;
#endif
#endif
#if  FTR_SIMPLIFIED_MV_COST
#if CLN_RATE_EST_CTRLS
    uint8_t approx_inter_rate; // use approximate rate for inter cost (set at pic-level b/c some pic-level initializations will be removed)
#else
    uint8_t use_low_precision_cost_estimation;
#endif
#endif
#if SS_FIX_MOVE_SKIP_INTRA_PIC
    uint8_t skip_intra;
#endif
} PictureControlSet;

// To optimize based on the max input size
// To study speed-memory trade-offs
typedef struct SbParams {
    uint8_t  horizontal_index;
    uint8_t  vertical_index;
    uint16_t origin_x;
    uint16_t origin_y;
    uint8_t  width;
    uint8_t  height;
    uint8_t  is_complete_sb;
    EbBool   raster_scan_blk_validity[CU_MAX_COUNT];
    uint8_t  is_edge_sb;
    uint32_t tile_start_x;
    uint32_t tile_start_y;
    uint32_t tile_end_x;
    uint32_t tile_end_y;
} SbParams;

typedef struct SbGeom {
    uint16_t horizontal_index;
    uint16_t vertical_index;
    uint16_t origin_x;
    uint16_t origin_y;
    uint8_t  width;
    uint8_t  height;
    uint8_t  is_complete_sb;
    EbBool   block_is_inside_md_scan[BLOCK_MAX_COUNT_SB_128];
    EbBool   block_is_allowed[BLOCK_MAX_COUNT_SB_128];
} SbGeom;

typedef struct TileGroupInfo {
    uint16_t tile_group_sb_start_x;
    uint16_t tile_group_sb_start_y;
    uint16_t tile_group_sb_end_x;
    uint16_t tile_group_sb_end_y;
    uint16_t tile_group_width_in_sb;
    uint16_t tile_group_height_in_sb;

    uint16_t tile_group_tile_start_x;
    uint16_t tile_group_tile_start_y;
    uint16_t tile_group_tile_end_x;
    uint16_t tile_group_tile_end_y;
} TileGroupInfo;
typedef struct MotionEstimationData {
    EbDctor       dctor;
    MeSbResults **me_results;
    uint16_t      sb_total_count_unscaled;
#if OPT_ME
    uint8_t       max_cand;//total max me candidates given the active references
    uint8_t       max_refs;//total max active references
    uint8_t       max_l0;  //max active refs in L0
#endif
#if OPT_TPL_DATA
    OisMbResults **       ois_mb_results;
    TplStats **           tpl_stats;

    TplSrcStats *         tpl_src_stats;      //tpl src based stats

    int32_t               base_rdmult;
    double *              tpl_beta;
    double *              tpl_rdmult_scaling_factors;
    double *              tpl_sb_rdmult_scaling_factors;
#endif
} MotionEstimationData;
typedef struct TplControls {
    uint8_t tpl_opt_flag; // 0:OFF 1:ON - TPL optimizations : no rate, only DC
    uint8_t enable_tpl_qps; // 0:OFF 1:ON - QPS in TPL
    uint8_t disable_intra_pred_nref; // 0:OFF 1:ON - Disable intra prediction in NREF
#if SS_OPT_TPL
    uint8_t disable_intra_pred_nbase; // Depricated . to remove
#else
    uint8_t disable_intra_pred_nbase; // 0:OFF 1:ON - Disable intra prediction in NBASE
#endif
    uint8_t disable_tpl_nref; // 0:OFF 1:ON - Disable tpl in NREF
    uint8_t disable_tpl_pic_dist; // 16: OFF - 0: ON
#if SS_OPT_TPL
    uint8_t get_best_ref; //Depricated.to remove
#else
    uint8_t get_best_ref; // Reference pruning, get best reference
#endif
    EB_TRANS_COEFF_SHAPE pf_shape;
    uint8_t use_pred_sad_in_intra_search;
    uint8_t use_pred_sad_in_inter_search;
#if OPT5_TPL_REDUCE_PIC
    int8_t reduced_tpl_group;
#else
    uint8_t reduced_tpl_group;
#endif
    uint8_t skip_rdoq_uv_qp_based_th;
    double r0_adjust_factor;
#if FTR_QP_BASED_DEPTH_REMOVAL && !CLN_TPL_LEVEL_7
    uint8_t modulate_depth_removal_level; // Modulate depth_removal level @ BASE based on the qp_offset band (towards better quality only)
#endif
#if OPT_TPL_64X64_32X32
    uint8_t dispenser_search_level; // 0: use 16x16 block(s), 1: use 32x32 block(s), 2: use 64x64 block(s)  (for incomplete 64x64, dispenser_search_level is set to 0)
                                    // it is recommended to use subsample_tx=2, when dispenser_search_level is set to 1
#endif
#if FTR_TPL_TX_SUBSAMPLE
#if OPT_TPL_64X64_32X32
    uint8_t subsample_tx; // 0: OFF, use full TX size; 1: subsample the transforms in TPL by 2; 2: subsample the transforms in TPL by 4
#else
    uint8_t subsample_tx; // 0: OFF, use full TX size; 1: subsample the transforms in TPL by 2
#endif
#endif
#if FTR_16X16_TPL_MAP
    uint8_t synth_blk_size; //syntheszier block size, support 8x8 and 16x16 for now. NOTE: this field must be
                            //modified inside the get_ function, as it is linked to memory allocation at init time
#endif
#if ADJUST_LAMBDA
    uint8_t vq_adjust_lambda_sb;
#endif
} TplControls;

/*!
 * \brief Refresh frame flags for different type of frames.
 *
 * If the refresh flag is true for a particular reference frame, after the
 * current frame is encoded, the reference frame gets refreshed (updated) to
 * be the current frame. Note: Usually at most one flag will be set to true at
 * a time. But, for key-frames, all flags are set to true at once.
 */
typedef struct {
    bool golden_frame; /*!< Refresh flag for golden frame */
    bool bwd_ref_frame; /*!< Refresh flag for bwd-ref frame */
    bool alt_ref_frame; /*!< Refresh flag for alt-ref frame */
} RefreshFrameFlagsInfo;

typedef struct {
    uint8_t                     tpl_temporal_layer_index;
    EB_SLICE                    tpl_slice_type;
    uint8_t                     tpl_ref0_count;
    uint8_t                     tpl_ref1_count;
    uint64_t                    tpl_decode_order;
    EbBool                      ref_in_slide_window[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
#if FIX_TPL_NON_VALID_REF
    int32_t                     ref_tpl_group_idx[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];//tpl group index of all ref pictures; -1 if the reference is not in the TPL group (aka sliding window)
    struct PictureParentControlSet     *base_pcs;//TPL base picture
#endif
    EbBool                      is_used_as_reference_flag;
    EbDownScaledBufDescPtrArray tpl_ref_ds_ptr_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
} TPLData;
#if !CLN_INTRA_CTRLS
#if TUNE_INTRA_LEVELS
typedef struct IntraPredControls {
    uint8_t disable_angle_prediction; // disables all angular prediction modes, including H_PRED and V_PRED
    uint8_t angle_delta_candidate_count; // enables refinement for the nominal directional intra prediction modes, can only take values of 0 or 1
    uint8_t dc_h_v_smooth; // generates only dc, horizontal, vertical, and smooth intra predicitons
    uint8_t limit_refinement; // limits angular prediction refinements to 2 candidates at -3 and +3 refinement steps
    uint8_t intra_mode_end; // the last intra prediciton mode generated starting from DC_PRED, min: DC_PRED, max: PAETH_PRED
    uint8_t skip_paeth; // skip PAETH_PRED mode
} IntraPredControls;
#endif
#endif
typedef struct GmControls {
    uint8_t enabled;
    uint8_t
            identiy_exit; // 0: generate GM params for both list_0 and list_1, 1: do not generate GM params for list_1 if list_0/ref_idx_0 is id
    uint8_t rotzoom_model_only; // 0: use both rotzoom and affine models, 1:use rotzoom model only
    uint8_t bipred_only; // 0: test both unipred and bipred, 1: test bipred only
    uint8_t bypass_based_on_me;
    uint8_t use_stationary_block; // 0: do not consider stationary_block info @ me-based bypass, 1: consider stationary_block info @ me-based bypass (only if bypass_based_on_me=1)
    uint8_t use_distance_based_active_th; // 0: used default active_th,1: increase active_th baed on distance to ref (only if bypass_based_on_me=1)
#if FTR_LIMIT_GM_REFINEMENT
    uint8_t params_refinement_steps; // The number of refinement steps to use in the GM params refinement
#endif
#if CLN_ME_SIGS
    uint8_t downsample_level; // GM_FULL: Exhaustive search mode; GM_DOWN: Downsampled resolution with a downsampling factor of 2 in each dimension; GM_TRAN_ONLY: Translation only using ME MV.
#endif
} GmControls;
#if FTR_MULTI_STAGE_CDEF
typedef struct CdefControls {
    uint8_t enabled;
    uint8_t number_of_prim_in_second_loop[2];
    uint8_t first_pass_fs_num;
    uint8_t default_first_pass_fs[TOTAL_STRENGTHS];
    uint8_t default_second_pass_fs_num;
    uint8_t default_second_pass_fs[TOTAL_STRENGTHS];
#if MS_CDEF_OPT2
    int8_t default_first_pass_fs_uv[TOTAL_STRENGTHS];
    int8_t default_second_pass_fs_uv[TOTAL_STRENGTHS];
#endif
#if MS_CDEF_OPT3
    int8_t use_reference_cdef_fs;
    int8_t pred_y_f;
    int8_t pred_uv_f;
#endif
#if FTR_CDEF_SUBSAMPLING
    uint8_t subsampling_factor; // Allowable levels: [1,2,4] ---> 1: no subsampling; 2: process every 2nd row; 4: process every 4th row for 8x8 blocks, every 2nd row for smaller sizes.
                                // NB subsampling is capped for certain block sizes, based on how many points the intrinsics can process at once.
#endif
#if FTR_CDEF_SEARCH_BEST_REF
    uint8_t search_best_ref_fs; // Only search best filter strengths of the nearest ref frames (skips the search if the filters of list0/list1 are the same).
#endif
#if FTR_CDEF_BIAS_ZERO_COST
    uint16_t zero_fs_cost_bias; // 0: OFF, higher is safer. Scaling factor to decrease the zero filter strength cost: : <x>/64
#endif
#if OPT_CDEF
    uint8_t use_skip_detector;
#endif
} CdefControls;
#endif



#if CLN_ADD_LIST0_ONLY_CTRL
typedef struct List0OnlyBase {
    uint8_t enabled;
    uint16_t noise_variance_th;
    uint32_t ahd_mult;
} List0OnlyBase;
#endif
#if CLN_DLF_SIGNALS
typedef struct DlfCtrls {
    uint8_t enabled;
    uint8_t sb_based_dlf;           // if true, perform DLF per SB, not per picture
} DlfCtrls;
#endif
#if OPT_IBC_HASH_SEARCH
typedef struct IntraBCCtrls {
    uint8_t enabled;
    uint8_t ibc_shift;              // Shift for full_pixel_exhaustive search threshold:   0: No Shift   1:Shift to left by 1
    uint8_t ibc_direction;          // search directions: 0: Left + Top      1: Top only
    uint8_t hash_4x4_blocks;        // this will be set by get_disallow_4x4, will not hash 4x4 blocks for higher presets where 4x4 blocks are not encoded
    uint8_t max_block_size_hash;    // the maximum block size that will be hashed, corresponds to the maximum block size for which a MD candidate will be generated by IBC hashing algorithm
} IntraBCCtrls;
#endif

//CHKN
// Add the concept of PictureParentControlSet which is a subset of the old PictureControlSet.
// It actually holds only high level Picture based control data:(GOP management,when to start a picture, when to release the PCS, ....).
// The regular PictureControlSet(Child) will be dedicated to store SB based encoding results and information.
// Parent is created before the Child, and continue to live more. Child PCS only lives the exact time needed to encode the picture: from ME to EC/ALF.
typedef struct PictureParentControlSet {
    EbDctor              dctor;
    EbObjectWrapper *    scs_wrapper_ptr;
    EbObjectWrapper *    input_picture_wrapper_ptr;
#if OPT_PA_REF
    EbObjectWrapper *    eb_y8b_wrapper_ptr;
#endif
    EbObjectWrapper *    reference_picture_wrapper_ptr;
    EbObjectWrapper *    pa_reference_picture_wrapper_ptr;
    EbPictureBufferDesc *enhanced_picture_ptr;
    EbPictureBufferDesc *enhanced_downscaled_picture_ptr;
    EbPictureBufferDesc *enhanced_unscaled_picture_ptr;
    EbPictureBufferDesc
        *  chroma_downsampled_picture_ptr; //if 422/444 input, down sample to 420 for MD
    EbBool is_chroma_downsampled_picture_ptr_owner;
    PredictionStructure *      pred_struct_ptr; // need to check
    struct SequenceControlSet *scs_ptr;
    EbObjectWrapper *          p_pcs_wrapper_ptr;
    EbObjectWrapper *          previous_picture_control_set_wrapper_ptr;
    EbObjectWrapper *          output_stream_wrapper_ptr;
    Av1Common *                av1_cm;

    uint8_t             hbd_mode_decision;
    // Data attached to the picture. This includes data passed from the application, or other data the encoder attaches
    // to the picture.
    EbLinkedListNode *data_ll_head_ptr;
    // pointer to data to be passed back to the application when picture encoding is done
    EbLinkedListNode *app_out_data_ll_head_ptr;

    EbBufferHeaderType *input_ptr; // input picture buffer
    uint8_t             log2_tile_rows;
    uint8_t             log2_tile_cols;
    uint8_t             log2_sb_sz;
    TileGroupInfo *     tile_group_info;
    uint8_t             tile_group_cols;
    uint8_t             tile_group_rows;

    EbBool          idr_flag;
    EbBool          cra_flag;
    EbBool          scene_change_flag;
    EbBool          end_of_sequence_flag;
    uint8_t         picture_qp;
    uint64_t        picture_number;
    uint8_t         skip_frame;
    uint32_t        cur_order_hint;
    uint32_t        ref_order_hint[7];
    EB_SLICE        slice_type;
    uint8_t         pred_struct_index;
    uint8_t         temporal_layer_index;
    uint64_t        decode_order;
    EbBool          is_used_as_reference_flag;
#if FIX_I51
    uint8_t          reference_released; // status of PA reference 0: Not release; 1: Released
#endif
    uint8_t         ref_list0_count;
    uint8_t         ref_list1_count;
    uint8_t
        ref_list0_count_try; //The number of references to try (in ME / MD) in list0.Should be <= ref_list0_count.
    uint8_t
                     ref_list1_count_try; // The number of references to try (in ME/MD) in list1. Should be <= ref_list1_count.
    MvReferenceFrame ref_frame_type_arr[MODE_CTX_REF_FRAMES];
    uint8_t          tot_ref_frame_types;
    // Rate Control
    uint64_t total_num_bits;
    uint16_t sb_total_count;
    EbBool   end_of_sequence_region;
#if FTR_1PAS_VBR
    int  frames_in_sw; // used for Look ahead
    struct RateControlIntervalParamContext *rate_control_param_ptr;
#else
    uint8_t  frames_in_sw; // used for Look ahead
#endif
    EbBool   qp_on_the_fly;
#if !RFCTR_RC_P2
    uint8_t  calculated_qp;
    uint8_t  intra_selected_org_qp;
    uint64_t sad_me;
    uint64_t quantized_coeff_num_bits;
    uint64_t average_qp;
#endif
    uint64_t last_idr_picture;
    uint64_t start_time_seconds;
    uint64_t start_time_u_seconds;
    uint32_t luma_sse;
    uint32_t cr_sse;
    uint32_t cb_sse;
    double   luma_ssim;
    double   cr_ssim;
    double   cb_ssim;

    EbObjectWrapper *           down_scaled_picture_wrapper_ptr;
    EbDownScaledBufDescPtrArray ds_pics; // Pointer array for down scaled pictures

    TPLData tpl_data;
#if OPT_PA_REF
    EbObjectWrapper *ref_y8b_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
#endif
    // Pre Analysis
    EbObjectWrapper *ref_pa_pic_ptr_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
    uint64_t         ref_pic_poc_array[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
    uint16_t **      variance;
    uint32_t         pre_assignment_buffer_count;
    uint16_t         pic_avg_variance;
    EbBool           scene_transition_flag[MAX_NUM_OF_REF_PIC_LIST];
    uint8_t          average_intensity[3];
    // Non moving index array
    uint8_t *non_moving_index_array;
    int      kf_zeromotion_pct; // percent of zero motion blocks
    uint8_t  fade_out_from_black;
    uint8_t  fade_in_to_black;
    uint16_t non_moving_index_average; // used by ModeDecisionConfigurationProcess()
    int16_t  non_moving_index_min_distance;
    int16_t  non_moving_index_max_distance;
    uint16_t qp_scaling_average_complexity;
    // Histograms
    uint32_t ****picture_histogram;
    uint64_t     average_intensity_per_region[MAX_NUMBER_OF_REGIONS_IN_WIDTH]
                                         [MAX_NUMBER_OF_REGIONS_IN_HEIGHT][3];

    // Segments
    uint16_t me_segments_total_count;
    uint8_t  me_segments_column_count;
    uint8_t  me_segments_row_count;
    uint16_t me_segments_completion_count;

    EbPictureBufferDesc       *non_tf_input;
    // Motion Estimation Results
    uint8_t   max_number_of_pus_per_sb;
    uint32_t *rc_me_distortion;
    uint8_t *     stationary_block_present_sb; // 1 when a % of the SB is stationary relative to reference frame(s) ((0,0) MV: decode order), 0 otherwise
    uint8_t *     rc_me_allow_gm;



    uint32_t *me_8x8_cost_variance;
    uint32_t *me_64x64_distortion;
    uint32_t *me_32x32_distortion;
    uint32_t *me_16x16_distortion;
    uint32_t *me_8x8_distortion;
    // Global motion estimation results
    EbBool               is_global_motion[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];
    EbWarpedMotionParams global_motion_estimation[MAX_NUM_OF_REF_PIC_LIST][REF_LIST_MAX_DEPTH];

    uint16_t              me_processed_sb_count;
    EbHandle              me_processed_sb_mutex;
    FirstPassData         firstpass_data;
    RefreshFrameFlagsInfo refresh_frame;
#if !RFCTR_RC_P1
    int                   internal_altref_allowed;
#endif
#if FTR_2PASS_1PASS_UNIFICATION
    double                ts_duration;
#else
    int64_t               ts_duration;
#endif
#if OPT_TPL_DATA
    double                r0;
    uint8_t               tpl_src_data_ready; //track pictures that are processd in two different TPL groups
#else
    OisMbResults **       ois_mb_results;
    TplStats **           tpl_stats;
#if SS_OPT_TPL
    TplSrcStats *         tpl_src_stats;      //tpl src based stats
    uint8_t               tpl_src_data_ready; //track pictures that are processd in two different TPL groups
#endif
#if !FTR_TPL_SYNTH
    int32_t               is_720p_or_larger;
#endif
    int32_t               base_rdmult;
    double                r0;
    double *              tpl_beta;
    double *              tpl_rdmult_scaling_factors;
    double *              tpl_sb_rdmult_scaling_factors;
#endif
    EbBool                blk_lambda_tuning;
    // Dynamic GOP
    EbPred   pred_structure;
    uint8_t  hierarchical_levels;
    EbBool   init_pred_struct_position_flag;
    int8_t   hierarchical_layers_diff;
    //Dep-Cnt Clean up is done using 2 mechanism
    //1: a triggering picture that will clean up all previous pictures;
    //2: a picture does a self clean up
    int32_t
        self_updated_links; // if negative: number of pic not dependent on curr; usefull for pictures in current MG which have a dec order > Base-Intra
        //due to I frame Insertion
    DepCntPicInfo updated_links_arr
        [UPDATED_LINKS]; //if not empty, this picture is a depn-cnt-cleanUp triggering picture (I frame; or MG size change )
        //this array will store all others pictures needing a dep-cnt clean up.
    uint32_t
        other_updated_links_cnt; //how many other pictures in the above array needing a dep-cnt clean-up
    // HME Flags
    EbBool enable_hme_flag;
    EbBool enable_hme_level0_flag;
    EbBool enable_hme_level1_flag;
    EbBool enable_hme_level2_flag;

    // HME Flags form Temporal Filtering
    EbBool tf_enable_hme_flag;
    EbBool tf_enable_hme_level0_flag;
    EbBool tf_enable_hme_level1_flag;
    EbBool tf_enable_hme_level2_flag;

    // MD
    EbEncMode         enc_mode;
    EB_SB_DEPTH_MODE *sb_depth_mode_array;
    // Multi-modes signal(s)
    MultiPassPdLevel multi_pass_pd_level;
    EbBool           disallow_nsq;
    EbBool           disallow_all_nsq_blocks_below_8x8;
    EbBool           disallow_all_nsq_blocks_below_16x16;
    EbBool           disallow_all_non_hv_nsq_blocks_below_16x16;
    EbBool           disallow_all_h4_v4_blocks_below_16x16;

    EbBool  disallow_all_nsq_blocks_below_64x64; //disallow nsq in 64x64 and below
    EbBool  disallow_all_nsq_blocks_below_32x32; //disallow nsq in 32x32 and below
    EbBool  disallow_all_nsq_blocks_above_64x64; //disallow nsq in 64x64 and above
    EbBool  disallow_all_nsq_blocks_above_32x32; //disallow nsq in 32x32 and above
    EbBool  disallow_all_nsq_blocks_above_16x16; //disallow nsq in 16x16 and above
    EbBool  disallow_HV4; //disallow             H4/V4
    EbBool  disallow_HVA_HVB_HV4; //disallow HA/HB/VA/VB H4/V4
#if CLN_DLF_SIGNALS
    DlfCtrls dlf_ctrls;
#else
    uint8_t loop_filter_mode;
#endif
    uint8_t intra_pred_mode;
    uint8_t tx_size_search_mode;
    uint8_t frame_end_cdf_update_mode; // mm-signal: 0: OFF, 1:ON
    //**********************************************************************************************************//
    Av1RpsNode av1_ref_signal;
    EbBool     has_show_existing;
    int32_t    ref_frame_map[REF_FRAMES]; /* maps fb_idx to reference slot */
    int32_t    is_skip_mode_allowed;
    int32_t    skip_mode_flag;
    uint32_t   pic_index;  //index of picture in the mg
    // Flag for a frame used as a reference - not written to the Bitstream
    int32_t is_reference_frame;

    // Flag signaling that the frame is encoded using only INTRA modes.
    uint8_t intra_only;
    /* profile settings */
#if CONFIG_ENTROPY_STATS
    int32_t coef_cdf_category;
#endif
    int32_t separate_uv_delta_q;

    // Global quant matrix tables
    const QmVal *giqmatrix[NUM_QM_LEVELS][3][TX_SIZES_ALL];
    const QmVal *gqmatrix[NUM_QM_LEVELS][3][TX_SIZES_ALL];
    int32_t      min_qmlevel;
    int32_t      max_qmlevel;
    // Encoder
    LoopFilterInfoN lf_info;

    // Flag signaling how frame contexts should be updated at the end of
    // a frame decode
    RefreshFrameContextMode refresh_frame_context;
    uint32_t                frame_context_idx; /* Context to use/update */
    int32_t                 fb_of_context_type[REF_FRAMES];
    uint64_t                frame_offset;
    uint32_t                large_scale_tile;
    int32_t                 nb_cdef_strengths;
    // Resolution of delta quant
    int32_t monochrome;
    int32_t prev_qindex[MAX_TILE_CNTS];

    // Since actual frame level loop filtering level value is not available
    // at the beginning of the tile (only available during actual filtering)
    // at encoder side.we record the delta_lf (against the frame level loop
    // filtering level) and code the delta between previous superblock's delta
    // lf and current delta lf. It is equivalent to the delta between previous
    // superblock's actual lf and current lf.
    int32_t prev_delta_lf_from_base;
    // For this experiment, we have four frame filter levels for different plane
    // and direction. So, to support the per superblock update, we need to add
    // a few more params as below.
    // 0: delta loop filter level for y plane vertical
    // 1: delta loop filter level for y plane horizontal
    // 2: delta loop filter level for u plane
    // 3: delta loop filter level for v plane
    // To make it consistent with the reference to each filter level in segment,
    // we need to -1, since
    // SEG_LVL_ALT_LF_Y_V = 1;
    // SEG_LVL_ALT_LF_Y_H = 2;
    // SEG_LVL_ALT_LF_U   = 3;
    // SEG_LVL_ALT_LF_V   = 4;
    //
    int32_t prev_delta_lf[FRAME_LF_COUNT];
    int32_t curr_delta_lf[FRAME_LF_COUNT];

    // Resolution of delta quant
    // int32_t delta_q_res;
    int32_t              allow_comp_inter_inter;
    int16_t              pan_mvx;
    int16_t              pan_mvy;
    int16_t              tilt_mvx;
    int16_t              tilt_mvy;
    EbWarpedMotionParams global_motion[TOTAL_REFS_PER_FRAME];
    PictureControlSet *  child_pcs;
    EncDecSet *  enc_dec_ptr;
    Macroblock *         av1x;
    int32_t film_grain_params_present; //todo (AN): Do we need this flag at picture level?
    AomDenoiseAndModel *denoise_and_model;
    int8_t              cdef_level;
    uint8_t             palette_level;
    uint8_t             sc_class0;
    uint8_t             sc_class1;
    uint8_t             sc_class2;
#if !OPT_IBC_HASH_SEARCH
    uint8_t             ibc_mode;
#endif
    SkipModeInfo        skip_mode_info;
    uint64_t picture_number_alt; // The picture number overlay includes all the overlay frames
    uint8_t  is_alt_ref;
    uint8_t  is_overlay;
    struct PictureParentControlSet *overlay_ppcs_ptr;
    struct PictureParentControlSet *alt_ref_ppcs_ptr;
#if FIXED_POINTS_PLANEWISE
    int32_t                         noise_levels_log1p_fp16[MAX_MB_PLANE];
#endif /*FIXED_POINTS_PLANEWISE*/
    double                          noise_levels[MAX_MB_PLANE];
    int32_t                         pic_decision_reorder_queue_idx;
    struct PictureParentControlSet *temp_filt_pcs_list[ALTREF_MAX_NFRAMES];
    EbByte                          save_enhanced_picture_ptr[3];
    EbByte                          save_enhanced_picture_bit_inc_ptr[3];
    EbHandle                        temp_filt_done_semaphore;
    EbHandle                        temp_filt_mutex;
    EbHandle                        debug_mutex;

    uint8_t  temp_filt_prep_done;
    uint16_t temp_filt_seg_acc;
    EbHandle tpl_disp_done_semaphore;

    AtomicVarU32 pame_done; //set when PA ME is done.
    CondVar me_ready;

    int16_t      tf_segments_total_count;
    uint8_t      tf_segments_column_count;
    uint8_t      tf_segments_row_count;
    uint8_t      past_altref_nframes;
    uint8_t      future_altref_nframes;
    EbBool       temporal_filtering_on;
    uint64_t    filtered_sse_uv;
    FrameHeader frm_hdr;
    uint16_t *  altref_buffer_highbd[3];
    uint8_t     pic_obmc_level;
#if !CLN_ME_SIGS
    uint8_t     gm_level;
#endif
#if !OPT_TXS_SEARCH
    uint8_t     tx_size_early_exit;
#endif
    SbParams *        sb_params_array;
    SbGeom *          sb_geom;
    EbInputResolution input_resolution;
    uint16_t          picture_sb_width;
    uint16_t          picture_sb_height;
    uint16_t          sb_total_count_unscaled;

    // Picture dimensions (resized or not)
    // aligned to be a multiple of 8 pixels
    uint16_t aligned_width;
    uint16_t aligned_height;

    // Picture dimensions (resized or not)
    // --not-- aligned to be a multiple of 8 pixels
    uint16_t frame_width;
    uint16_t frame_height;

    EbBool                frame_superres_enabled;
    uint8_t               superres_denom;
    EbObjectWrapper *     me_data_wrapper_ptr;
    MotionEstimationData *pa_me_data;
    unsigned char         gf_group_index;
    struct PictureParentControlSet
        *    tpl_group[MAX_TPL_GROUP_SIZE]; //stores pcs pictures needed for tpl algorithm
    uint32_t tpl_group_size; //size of above buffer
    void *   pd_window
        [PD_WINDOW_SIZE]; //stores previous, current, future pictures from pd-reord-queue. empty for first I.
    uint8_t pd_window_count;

    struct PictureParentControlSet
        *ext_group[MAX_TPL_EXT_GROUP_SIZE]; //stores pcs pictures needed for lad mg based algorithms
    uint32_t ext_group_size; //actual size of extended group

    int64_t  ext_mg_id;
    int64_t  ext_mg_size; //same as mg expect for MGops with [LDP-I] which are split into 2

    struct PictureParentControlSet *ntpl_group[MAX_TPL_EXT_GROUP_SIZE]; //new tpl group formed the extended group
    uint8_t tpl_valid_pic[MAX_TPL_EXT_GROUP_SIZE];
    uint8_t used_tpl_frame_num;
    uint32_t ntpl_group_size;

    // Tune TPL for better chroma.Only for 240P
    uint8_t    tune_tpl_for_chroma;
    uint8_t    is_next_frame_intra;
    uint8_t is_superres_none;
    TfControls tf_ctrls;
    GmControls gm_ctrls;
#if !CLN_INTRA_CTRLS
#if TUNE_INTRA_LEVELS
    IntraPredControls intra_ctrls;
#endif
#endif
#if FTR_MULTI_STAGE_CDEF
    CdefControls cdef_ctrls;
#endif
    // RC related variables
    int                             q_low;
    int                             q_high;
    int                             loop_count;
    int                             overshoot_seen;
    int                             undershoot_seen;
    int                             low_cr_seen;
    uint64_t                        pcs_total_rate;
    EbHandle                        pcs_total_rate_mutex;
    int16_t                         first_pass_seg_total_count;
    uint8_t                         first_pass_seg_column_count;
    uint8_t                         first_pass_seg_row_count;
    uint16_t                        first_pass_seg_acc;
    EbHandle                        first_pass_done_semaphore;
    EbHandle                        first_pass_mutex;
    struct PictureParentControlSet *first_pass_ref_ppcs_ptr[2];
    uint8_t                         first_pass_ref_count;
    uint8_t                         first_pass_done;
#if FTR_OPT_MPASS_BYPASS_FRAMES
    uint8_t                         first_frame_in_minigop;
#endif
    TplControls                     tpl_ctrls;
    uint8_t tpl_is_valid;
#if CLN_ADD_LIST0_ONLY_CTRL
    List0OnlyBase list0_only_base_ctrls;
#else
    uint8_t                         list0_only_base; // Use list0 only if BASE (mimik a P)
#endif

    EbHandle tpl_disp_mutex;
    //uint32_t         input_type;
    int16_t          enc_dec_segment_row;
    uint16_t         tile_group_index;
    uint16_t         tpl_disp_coded_sb_count;

    uint16_t sb_total_count_pix;
    EncDecSegments **tpl_disp_segment_ctrl;
    // the offsets for STATS_BUFFER_CTX
    uint64_t                        stats_in_end_offset;
    // the offsets for stats_in
    uint64_t                        stats_in_offset;
    //GF_GROUP parameters store in PCS
    int                             update_type;
    int                             layer_depth;
    int                             arf_boost;
    int                             gf_group_size;
    //RATE_CONTROL parameters store in PCS
    int                             base_frame_target; // A baseline frame target before adjustment.
    int                             this_frame_target; // Actual frame target after rc adjustment.
    int                             projected_frame_size;
#if FTR_RC_CAP
    int                             max_frame_size;
#endif
    int                             frames_to_key;
    int                             frames_since_key;
    int                             is_src_frame_alt_ref;
    // Total number of stats used only for gfu_boost calculation.
    int                             num_stats_used_for_gfu_boost;
    // Total number of stats required by gfu_boost calculation.
    int                             num_stats_required_for_gfu_boost;
    int                             top_index;
    int                             bottom_index;
    // stores gf group (minigop) length
    int                             gf_interval;
    int                             gf_update_due; // thr gf update in RC is due for I, or base frames (except the one after I) or P frames
    uint8_t                         is_new_gf_group;
    struct PictureParentControlSet *gf_group[MAX_TPL_GROUP_SIZE];
#if FTR_NEW_MULTI_PASS
    StatStruct                      stat_struct;
#endif
#if OPT9_RATE_ESTIMATION
    uint8_t partition_contexts;
#endif
    uint8_t bypass_cost_table_gen;
#if TUNE_MDS0
    uint16_t max_can_count;
#endif
#if ME_8X8
    uint8_t enable_me_8x8;
#endif
#if FTR_M13
    uint8_t enable_me_16x16;
#endif
#if FTR_MVP_BEST_ME_LIST
    uint8_t use_best_me_unipred_cand_only; // if MRP is OFF, use one ME unipred candidate only
#endif
#if OPT_IBC_HASH_SEARCH
    IntraBCCtrls intraBC_ctrls;
#endif
#if OPT_FIRST_PASS3
    uint8_t bypass_blk_step;
#endif

#if OPT_PREHME
    uint32_t  tf_tot_vert_blks;    //total vertical motion blocks in TF
    uint32_t  tf_tot_horz_blks;    //total horizontal motion blocks in TF
    int8_t    tf_motion_direction; //motion direction in TF   -1:invalid   0:horz  1:vert
#endif
#if FTR_NEW_QPS
    uint8_t cqp_qps_model; // 0: use fixed QP-Offsets, 1: QP-Offsets are funtion of the base_qp
#endif
} PictureParentControlSet;


typedef struct TplDispResults {
    EbDctor          dctor;
    EbObjectWrapper *pcs_wrapper_ptr;
    uint32_t        frame_index;
    EbFifo *sbo_feedback_fifo_ptr;
    uint32_t  input_type;
    int16_t   enc_dec_segment_row;
    uint16_t  tile_group_index;
    PictureParentControlSet* pcs_ptr;
    int32_t         qIndex;

} TplDispResults;


typedef struct PictureControlSetInitData {
    uint16_t       picture_width;
    uint16_t       picture_height;
    uint16_t       left_padding;
    uint16_t       right_padding;
    uint16_t       top_padding;
    uint16_t       bot_padding;
    EbBitDepthEnum bit_depth;
    EbColorFormat  color_format;
    uint32_t       sb_sz;
    uint8_t        cfg_palette;
    uint32_t
        sb_size_pix; //since we still have lot of code assuming 64x64 SB, we add a new paramter supporting both128x128 and 64x64,
    //ultimately the fixed code supporting 64x64 should be upgraded to use 128x128 and the above could be removed.
    uint32_t max_depth;
    //EbBool                             is_16bit;
    uint32_t  ten_bit_format;
    uint32_t  compressed_ten_bit_format;
    uint16_t  enc_dec_segment_col;
    uint16_t  enc_dec_segment_row;
    EbEncMode enc_mode;
    EbSvtAv1EncConfiguration static_config;
    uint8_t   speed_control;
    int8_t    hbd_mode_decision;
    uint16_t  film_grain_noise_level;
    EbBool    ext_block_flag;
    uint8_t   cdf_mode;
    uint8_t   over_boundary_block_mode;
    uint8_t   mfmv;
    //init value for child pcs
    uint8_t tile_row_count;
    uint8_t tile_column_count;

    //Init value for parent pcs
    uint8_t log2_tile_rows; //from command line
    uint8_t log2_tile_cols;
    uint8_t log2_sb_sz; //in mi unit
    EbBool  is_16bit_pipeline;

    uint16_t non_m8_pad_w;
    uint16_t non_m8_pad_h;
    uint8_t  enable_tpl_la;
#if FTR_16X16_TPL_MAP
    uint8_t  tpl_synth_size;
#endif
    uint8_t  in_loop_ois;

    uint8_t  rc_firstpass_stats_out;
    uint32_t rate_control_mode;
#if TUNE_MULTI_PASS
    uint8_t    passes;
#endif
    Av1Common *                av1_cm;
#if CLN_GEOM
    uint16_t   init_max_block_cnt;
#endif

#if OPT_ME
   uint8_t mrp_level;
#endif
#if SS_MEM_VAR

  uint8_t  enable_adaptive_quantization;
#endif
#if SS_MEM_HIS

  uint8_t  scene_change_detection;
#endif
#if SS_MEM_TPL
#if FTR_LAD_INPUT
  uint8_t tpl_lad_mg;
#else
  uint8_t lad_mg;
#endif
#endif
#if FIX_DG
  uint8_t skip_frame_first_pass;
  uint8_t ipp_ds;
  uint8_t bypass_blk_step;
  uint8_t dist_ds;
  uint8_t ipp_was_ds;
#if IPP_CTRL
  uint8_t final_pass_preset;
  uint8_t bypass_zz_check;
  uint8_t use8blk;
  uint8_t reduce_me_search;
#endif
#endif
#if TUNE_MEM_SHUT
  uint8_t       input_resolution;
#endif
} PictureControlSetInitData;

typedef struct Av1Comp {
    Yv12BufferConfig trial_frame_rst;
} Av1Comp;

/**************************************
     * Extern Function Declarations
     **************************************/

#if FTR_16K
uint32_t  get_out_buffer_size(uint32_t picture_width, uint32_t picture_height);
#endif

extern EbErrorType picture_control_set_creator(EbPtr *object_dbl_ptr, EbPtr object_init_data_ptr);
extern EbErrorType recon_coef_creator(EbPtr *object_dbl_ptr, EbPtr object_init_data_ptr);
extern EbErrorType picture_parent_control_set_creator(EbPtr *object_dbl_ptr,
                                                      EbPtr  object_init_data_ptr);
extern EbErrorType me_creator(EbPtr *object_dbl_ptr, EbPtr object_init_data_ptr);
#if  OPT_ME
extern EbErrorType me_sb_results_ctor(MeSbResults *obj_ptr,PictureControlSetInitData *init_data_ptr);
#else
extern EbErrorType me_sb_results_ctor(MeSbResults *obj_ptr);
#endif
#ifdef __cplusplus
}
#endif
#endif // EbPictureControlSet_h
