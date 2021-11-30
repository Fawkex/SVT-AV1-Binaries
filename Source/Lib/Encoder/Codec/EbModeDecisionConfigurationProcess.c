/*
* Copyright(c) 2019 Intel Corporation
* Copyright (c) 2016, Alliance for Open Media. All rights reserved
*
* This source code is subject to the terms of the BSD 2 Clause License and
* the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
* was not distributed with this source code in the LICENSE file, you can
* obtain it at https://www.aomedia.org/license/software-license. If the Alliance for Open
* Media Patent License 1.0 was not distributed with this source code in the
* PATENTS file, you can obtain it at https://www.aomedia.org/license/patent-license.
*/

#include <stdlib.h>

#include "EbEncHandle.h"
#include "EbUtility.h"
#include "EbPictureControlSet.h"
#include "EbModeDecisionConfigurationProcess.h"
#include "EbRateControlResults.h"
#include "EbEncDecTasks.h"
#include "EbReferenceObject.h"
#include "EbModeDecisionProcess.h"
#include "av1me.h"
#include "EbQMatrices.h"
#include "EbLog.h"
#include "EbCoefficients.h"
#include "EbCommonUtils.h"
#include "EbResize.h"

int32_t get_qzbin_factor(int32_t q, AomBitDepth bit_depth);
void    invert_quant(int16_t *quant, int16_t *shift, int32_t d);
int16_t svt_av1_dc_quant_q3(int32_t qindex, int32_t delta, AomBitDepth bit_depth);
int16_t svt_av1_ac_quant_q3(int32_t qindex, int32_t delta, AomBitDepth bit_depth);
int16_t svt_av1_dc_quant_qtx(int32_t qindex, int32_t delta, AomBitDepth bit_depth);
#if CLN_4X4_SIG
uint8_t get_disallow_4x4(EbEncMode enc_mode, EB_SLICE slice_type);
#endif
#if CLN_BYP_ED_SIG
uint8_t  get_bypass_encdec(EbEncMode enc_mode, uint8_t hbd_mode_decision, uint8_t encoder_bit_depth);
#endif
#if CLN_DISALLOW_BELOW_16X16_SIG
#if CLN_RES_DISALLOW_B16
uint8_t get_disallow_below_16x16_picture_level(EbEncMode enc_mode, EbInputResolution resolution, EB_SLICE slice_type, uint8_t sc_class1, uint8_t is_used_as_reference_flag, uint8_t temporal_layer_index);
#else
uint8_t get_disallow_below_16x16_picture_level(EbEncMode enc_mode, EbInputResolution resolution, EB_SLICE slice_type, uint8_t sc_class1);
#endif
#endif

#define MAX_MESH_SPEED 5 // Max speed setting for mesh motion method
static MeshPattern good_quality_mesh_patterns[MAX_MESH_SPEED + 1][MAX_MESH_STEP] = {
    {{64, 8}, {28, 4}, {15, 1}, {7, 1}},
    {{64, 8}, {28, 4}, {15, 1}, {7, 1}},
    {{64, 8}, {14, 2}, {7, 1}, {7, 1}},
    {{64, 16}, {24, 8}, {12, 4}, {7, 1}},
    {{64, 16}, {24, 8}, {12, 4}, {7, 1}},
    {{64, 16}, {24, 8}, {12, 4}, {7, 1}},
};
static unsigned char good_quality_max_mesh_pct[MAX_MESH_SPEED + 1] = {50, 50, 25, 15, 5, 1};
// TODO: These settings are pretty relaxed, tune them for
// each speed setting
static MeshPattern intrabc_mesh_patterns[MAX_MESH_SPEED + 1][MAX_MESH_STEP] = {
    {{256, 1}, {256, 1}, {0, 0}, {0, 0}},
    {{256, 1}, {256, 1}, {0, 0}, {0, 0}},
    {{64, 1}, {64, 1}, {0, 0}, {0, 0}},
    {{64, 1}, {64, 1}, {0, 0}, {0, 0}},
    {{64, 4}, {16, 1}, {0, 0}, {0, 0}},
    {{64, 4}, {16, 1}, {0, 0}, {0, 0}},
};
static uint8_t intrabc_max_mesh_pct[MAX_MESH_SPEED + 1] = {100, 100, 100, 25, 25, 10};

// Adaptive Depth Partitioning
// Shooting states
#define UNDER_SHOOTING 0
#define OVER_SHOOTING 1
#define TBD_SHOOTING 2

#define SB_PRED_OPEN_LOOP_COST 100 // Let's assume PRED_OPEN_LOOP_COST costs ~100 U
#define U_101 101
#define U_102 102
#define U_103 103
#define U_104 104
#define U_105 105
#define U_107 107
#define SB_FAST_OPEN_LOOP_COST 108
#define U_109 109
#define SB_OPEN_LOOP_COST 110 // F_MDC is ~10% slower than PRED_OPEN_LOOP_COST
#define U_111 111
#define U_112 112
#define U_113 113
#define U_114 114
#define U_115 115
#define U_116 116
#define U_117 117
#define U_118 118
#define U_119 119
#define U_120 120
#define U_121 121
#define U_122 122
#define U_125 125
#define U_127 127
#define U_130 130
#define U_132 132
#define U_133 133
#define U_134 134
#define U_140 140
#define U_145 145
#define U_150 150
#define U_152 152
#define SQ_NON4_BLOCKS_SEARCH_COST 155
#define SQ_BLOCKS_SEARCH_COST 190
#define HIGH_SB_SCORE 60000
#define MEDIUM_SB_SCORE 16000
#define LOW_SB_SCORE 6000
#define MAX_LUMINOSITY_BOOST 10
int32_t budget_per_sb_boost[MAX_SUPPORTED_MODES] = {55, 55, 55, 55, 55, 55, 5, 5, 0};

void set_global_motion_field(PictureControlSet *pcs_ptr) {
    // Init Global Motion Vector
    uint8_t frame_index;
    for (frame_index = INTRA_FRAME; frame_index <= ALTREF_FRAME; ++frame_index) {
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmtype   = IDENTITY;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].alpha    = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].beta     = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].delta    = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].gamma    = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].invalid  = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[0] = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[1] = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[2] = (1 << WARPEDMODEL_PREC_BITS);
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[3] = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[4] = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[5] = (1 << WARPEDMODEL_PREC_BITS);
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[6] = 0;
        pcs_ptr->parent_pcs_ptr->global_motion[frame_index].wmmat[7] = 0;
    }

    //Update MV
    PictureParentControlSet *parent_pcs_ptr = pcs_ptr->parent_pcs_ptr;
    for (frame_index = INTRA_FRAME; frame_index <= ALTREF_FRAME; ++frame_index) {
        if (parent_pcs_ptr
                ->is_global_motion[get_list_idx(frame_index)][get_ref_frame_idx(frame_index)])
            parent_pcs_ptr->global_motion[frame_index] =
                parent_pcs_ptr->global_motion_estimation[get_list_idx(frame_index)]
                                                        [get_ref_frame_idx(frame_index)];

        // Upscale the translation parameters by 2, because the search is done on a down-sampled
        // version of the source picture (with a down-sampling factor of 2 in each dimension).
        if (parent_pcs_ptr->gm_level == GM_DOWN16) {
            parent_pcs_ptr->global_motion[frame_index].wmmat[0] *= 4;
            parent_pcs_ptr->global_motion[frame_index].wmmat[1] *= 4;
            parent_pcs_ptr->global_motion[frame_index].wmmat[0] = (int32_t)clamp(
                parent_pcs_ptr->global_motion[frame_index].wmmat[0],
                GM_TRANS_MIN * GM_TRANS_DECODE_FACTOR,
                GM_TRANS_MAX * GM_TRANS_DECODE_FACTOR);
            parent_pcs_ptr->global_motion[frame_index].wmmat[1] = (int32_t)clamp(
                parent_pcs_ptr->global_motion[frame_index].wmmat[1],
                GM_TRANS_MIN * GM_TRANS_DECODE_FACTOR,
                GM_TRANS_MAX * GM_TRANS_DECODE_FACTOR);
        } else if (parent_pcs_ptr->gm_level == GM_DOWN) {
            parent_pcs_ptr->global_motion[frame_index].wmmat[0] *= 2;
            parent_pcs_ptr->global_motion[frame_index].wmmat[1] *= 2;
            parent_pcs_ptr->global_motion[frame_index].wmmat[0] = (int32_t)clamp(
                parent_pcs_ptr->global_motion[frame_index].wmmat[0],
                GM_TRANS_MIN * GM_TRANS_DECODE_FACTOR,
                GM_TRANS_MAX * GM_TRANS_DECODE_FACTOR);
            parent_pcs_ptr->global_motion[frame_index].wmmat[1] = (int32_t)clamp(
                parent_pcs_ptr->global_motion[frame_index].wmmat[1],
                GM_TRANS_MIN * GM_TRANS_DECODE_FACTOR,
                GM_TRANS_MAX * GM_TRANS_DECODE_FACTOR);
        }
    }
}


void svt_av1_build_quantizer(AomBitDepth bit_depth, int32_t y_dc_delta_q, int32_t u_dc_delta_q,
                             int32_t u_ac_delta_q, int32_t v_dc_delta_q, int32_t v_ac_delta_q,
                             Quants *const quants, Dequants *const deq) {
    int32_t i, q, quant_q3, quant_qtx;

    for (q = 0; q < QINDEX_RANGE; q++) {
        const int32_t qzbin_factor     = get_qzbin_factor(q, bit_depth);
        const int32_t qrounding_factor = q == 0 ? 64 : 48;

        for (i = 0; i < 2; ++i) {
            int32_t qrounding_factor_fp = 64;
            // y quantizer setup with original coeff shift of Q3
            quant_q3 = i == 0 ? svt_av1_dc_quant_q3(q, y_dc_delta_q, bit_depth)
                              : svt_av1_ac_quant_q3(q, 0, bit_depth);
            // y quantizer with TX scale
            quant_qtx = i == 0 ? svt_av1_dc_quant_qtx(q, y_dc_delta_q, bit_depth)
                               : svt_av1_ac_quant_qtx(q, 0, bit_depth);
            invert_quant(&quants->y_quant[q][i], &quants->y_quant_shift[q][i], quant_qtx);
            quants->y_quant_fp[q][i] = (int16_t)((1 << 16) / quant_qtx);
            quants->y_round_fp[q][i] = (int16_t)((qrounding_factor_fp * quant_qtx) >> 7);
            quants->y_zbin[q][i]     = (int16_t)ROUND_POWER_OF_TWO(qzbin_factor * quant_qtx, 7);
            quants->y_round[q][i]    = (int16_t)((qrounding_factor * quant_qtx) >> 7);
            deq->y_dequant_qtx[q][i] = (int16_t)quant_qtx;
            deq->y_dequant_q3[q][i]  = (int16_t)quant_q3;

            // u quantizer setup with original coeff shift of Q3
            quant_q3 = i == 0 ? svt_av1_dc_quant_q3(q, u_dc_delta_q, bit_depth)
                              : svt_av1_ac_quant_q3(q, u_ac_delta_q, bit_depth);
            // u quantizer with TX scale
            quant_qtx = i == 0 ? svt_av1_dc_quant_qtx(q, u_dc_delta_q, bit_depth)
                               : svt_av1_ac_quant_qtx(q, u_ac_delta_q, bit_depth);
            invert_quant(&quants->u_quant[q][i], &quants->u_quant_shift[q][i], quant_qtx);
            quants->u_quant_fp[q][i] = (int16_t)((1 << 16) / quant_qtx);
            quants->u_round_fp[q][i] = (int16_t)((qrounding_factor_fp * quant_qtx) >> 7);
            quants->u_zbin[q][i]     = (int16_t)ROUND_POWER_OF_TWO(qzbin_factor * quant_qtx, 7);
            quants->u_round[q][i]    = (int16_t)((qrounding_factor * quant_qtx) >> 7);
            deq->u_dequant_qtx[q][i] = (int16_t)quant_qtx;
            deq->u_dequant_q3[q][i]  = (int16_t)quant_q3;

            // v quantizer setup with original coeff shift of Q3
            quant_q3 = i == 0 ? svt_av1_dc_quant_q3(q, v_dc_delta_q, bit_depth)
                              : svt_av1_ac_quant_q3(q, v_ac_delta_q, bit_depth);
            // v quantizer with TX scale
            quant_qtx = i == 0 ? svt_av1_dc_quant_qtx(q, v_dc_delta_q, bit_depth)
                               : svt_av1_ac_quant_qtx(q, v_ac_delta_q, bit_depth);
            invert_quant(&quants->v_quant[q][i], &quants->v_quant_shift[q][i], quant_qtx);
            quants->v_quant_fp[q][i] = (int16_t)((1 << 16) / quant_qtx);
            quants->v_round_fp[q][i] = (int16_t)((qrounding_factor_fp * quant_qtx) >> 7);
            quants->v_zbin[q][i]     = (int16_t)ROUND_POWER_OF_TWO(qzbin_factor * quant_qtx, 7);
            quants->v_round[q][i]    = (int16_t)((qrounding_factor * quant_qtx) >> 7);
            deq->v_dequant_qtx[q][i] = (int16_t)quant_qtx;
            deq->v_dequant_q3[q][i]  = (int16_t)quant_q3;
        }

        for (i = 2; i < 8; i++) { // 8: SIMD width
            quants->y_quant[q][i]       = quants->y_quant[q][1];
            quants->y_quant_fp[q][i]    = quants->y_quant_fp[q][1];
            quants->y_round_fp[q][i]    = quants->y_round_fp[q][1];
            quants->y_quant_shift[q][i] = quants->y_quant_shift[q][1];
            quants->y_zbin[q][i]        = quants->y_zbin[q][1];
            quants->y_round[q][i]       = quants->y_round[q][1];
            deq->y_dequant_qtx[q][i]    = deq->y_dequant_qtx[q][1];
            deq->y_dequant_q3[q][i]     = deq->y_dequant_q3[q][1];

            quants->u_quant[q][i]       = quants->u_quant[q][1];
            quants->u_quant_fp[q][i]    = quants->u_quant_fp[q][1];
            quants->u_round_fp[q][i]    = quants->u_round_fp[q][1];
            quants->u_quant_shift[q][i] = quants->u_quant_shift[q][1];
            quants->u_zbin[q][i]        = quants->u_zbin[q][1];
            quants->u_round[q][i]       = quants->u_round[q][1];
            deq->u_dequant_qtx[q][i]    = deq->u_dequant_qtx[q][1];
            deq->u_dequant_q3[q][i]     = deq->u_dequant_q3[q][1];
            quants->v_quant[q][i]       = quants->u_quant[q][1];
            quants->v_quant_fp[q][i]    = quants->v_quant_fp[q][1];
            quants->v_round_fp[q][i]    = quants->v_round_fp[q][1];
            quants->v_quant_shift[q][i] = quants->v_quant_shift[q][1];
            quants->v_zbin[q][i]        = quants->v_zbin[q][1];
            quants->v_round[q][i]       = quants->v_round[q][1];
            deq->v_dequant_qtx[q][i]    = deq->v_dequant_qtx[q][1];
            deq->v_dequant_q3[q][i]     = deq->v_dequant_q3[q][1];
        }
    }
}

void svt_av1_qm_init(PictureParentControlSet *pcs_ptr) {
    const uint8_t num_planes = 3; // MAX_MB_PLANE;// NM- No monochroma
    uint8_t       q, c, t;
    int32_t       current;
    for (q = 0; q < NUM_QM_LEVELS; ++q) {
        for (c = 0; c < num_planes; ++c) {
            current = 0;
            for (t = 0; t < TX_SIZES_ALL; ++t) {
                const int32_t size       = tx_size_2d[t];
                const TxSize  qm_tx_size = av1_get_adjusted_tx_size(t);
                if (q == NUM_QM_LEVELS - 1) {
                    pcs_ptr->gqmatrix[q][c][t]  = NULL;
                    pcs_ptr->giqmatrix[q][c][t] = NULL;
                } else if (t != qm_tx_size) { // Reuse matrices for 'qm_tx_size'
                    pcs_ptr->gqmatrix[q][c][t]  = pcs_ptr->gqmatrix[q][c][qm_tx_size];
                    pcs_ptr->giqmatrix[q][c][t] = pcs_ptr->giqmatrix[q][c][qm_tx_size];
                } else {
                    assert(current + size <= QM_TOTAL_SIZE);
                    pcs_ptr->gqmatrix[q][c][t]  = &wt_matrix_ref[q][c >= 1][current];
                    pcs_ptr->giqmatrix[q][c][t] = &iwt_matrix_ref[q][c >= 1][current];
                    current += size;
                }
            }
        }
    }
}

/******************************************************
* Set the reference sg ep for a given picture
******************************************************/
void set_reference_sg_ep(PictureControlSet *pcs_ptr) {
    Av1Common *        cm = pcs_ptr->parent_pcs_ptr->av1_cm;
    EbReferenceObject *ref_obj_l0, *ref_obj_l1;
    memset(cm->sg_frame_ep_cnt, 0, SGRPROJ_PARAMS * sizeof(int32_t));
    cm->sg_frame_ep = 0;

    // NADER: set cm->sg_ref_frame_ep[0] = cm->sg_ref_frame_ep[1] = -1 to perform all iterations
    switch (pcs_ptr->slice_type) {
    case I_SLICE:
        cm->sg_ref_frame_ep[0] = -1;
        cm->sg_ref_frame_ep[1] = -1;
        break;
    case B_SLICE:
        ref_obj_l0 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_0][0]->object_ptr;
        ref_obj_l1 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_1][0]->object_ptr;
        cm->sg_ref_frame_ep[0] = ref_obj_l0->sg_frame_ep;
        cm->sg_ref_frame_ep[1] = ref_obj_l1->sg_frame_ep;
        break;
    case P_SLICE:
        ref_obj_l0 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_0][0]->object_ptr;
        cm->sg_ref_frame_ep[0] = ref_obj_l0->sg_frame_ep;
        cm->sg_ref_frame_ep[1] = 0;
        break;
    default: SVT_LOG("SG: Not supported picture type"); break;
    }
}

void mode_decision_configuration_init_qp_update(PictureControlSet *pcs_ptr) {
    FrameHeader *frm_hdr                = &pcs_ptr->parent_pcs_ptr->frm_hdr;
#if !RFCTR_RC_P2
    pcs_ptr->parent_pcs_ptr->average_qp = 0;
#endif
#if  FTR_INTRA_DETECTOR
    pcs_ptr->intra_coded_area           = 0;
#endif
#if FTR_COEFF_DETECTOR
    pcs_ptr->skip_coded_area = 0;
#endif
    // Init block selection
    // Set reference sg ep
    set_reference_sg_ep(pcs_ptr);
    set_global_motion_field(pcs_ptr);

    svt_av1_qm_init(pcs_ptr->parent_pcs_ptr);
    MdRateEstimationContext *md_rate_estimation_array;

    md_rate_estimation_array = pcs_ptr->md_rate_estimation_array;

    if (pcs_ptr->parent_pcs_ptr->frm_hdr.primary_ref_frame != PRIMARY_REF_NONE)
        memcpy(&pcs_ptr->md_frame_context,
               &pcs_ptr->ref_frame_context[pcs_ptr->parent_pcs_ptr->frm_hdr.primary_ref_frame],
               sizeof(FRAME_CONTEXT));
    else {
        svt_av1_default_coef_probs(&pcs_ptr->md_frame_context,
                                   frm_hdr->quantization_params.base_q_idx);
        init_mode_probs(&pcs_ptr->md_frame_context);
    }
    // Initial Rate Estimation of the syntax elements
#if OPT8_MDC
    av1_estimate_syntax_rate(md_rate_estimation_array,
        pcs_ptr->slice_type == I_SLICE ? EB_TRUE : EB_FALSE,
        pcs_ptr->pic_filter_intra_level,
        pcs_ptr->parent_pcs_ptr->frm_hdr.allow_screen_content_tools,
        pcs_ptr->parent_pcs_ptr->scs_ptr->seq_header.enable_restoration,
        pcs_ptr->parent_pcs_ptr->frm_hdr.allow_intrabc,
#if OPT9_RATE_ESTIMATION
        pcs_ptr->parent_pcs_ptr->partition_contexts,
#endif
        &pcs_ptr->md_frame_context);
#else
    av1_estimate_syntax_rate(md_rate_estimation_array,
                             pcs_ptr->slice_type == I_SLICE ? EB_TRUE : EB_FALSE,
                             &pcs_ptr->md_frame_context);
#endif
    // Initial Rate Estimation of the Motion vectors
    av1_estimate_mv_rate(pcs_ptr, md_rate_estimation_array, &pcs_ptr->md_frame_context);
    // Initial Rate Estimation of the quantized coefficients
    av1_estimate_coefficients_rate(md_rate_estimation_array, &pcs_ptr->md_frame_context);
}

/******************************************************
* Compute Tc, and Beta offsets for a given picture
******************************************************/

static void mode_decision_configuration_context_dctor(EbPtr p) {
    EbThreadContext *                 thread_context_ptr = (EbThreadContext *)p;
    ModeDecisionConfigurationContext *obj                = (ModeDecisionConfigurationContext *)
                                                thread_context_ptr->priv;

#if !CLN_MDCONTEXT
    if (obj->is_md_rate_estimation_ptr_owner)
        EB_FREE_ARRAY(obj->md_rate_estimation_ptr);
#endif
    EB_FREE_ARRAY(obj);
}
/******************************************************
 * Mode Decision Configuration Context Constructor
 ******************************************************/
EbErrorType mode_decision_configuration_context_ctor(EbThreadContext *  thread_context_ptr,
                                                     const EbEncHandle *enc_handle_ptr,
                                                     int input_index, int output_index) {
    ModeDecisionConfigurationContext *context_ptr;
    EB_CALLOC_ARRAY(context_ptr, 1);
    thread_context_ptr->priv  = context_ptr;
    thread_context_ptr->dctor = mode_decision_configuration_context_dctor;

    // Input/Output System Resource Manager FIFOs
    context_ptr->rate_control_input_fifo_ptr = svt_system_resource_get_consumer_fifo(
        enc_handle_ptr->rate_control_results_resource_ptr, input_index);
    context_ptr->mode_decision_configuration_output_fifo_ptr =
        svt_system_resource_get_producer_fifo(enc_handle_ptr->enc_dec_tasks_resource_ptr,
                                              output_index);
#if !CLN_MDCONTEXT
    // Rate estimation
    EB_MALLOC_ARRAY(context_ptr->md_rate_estimation_ptr, 1);
    context_ptr->is_md_rate_estimation_ptr_owner = EB_TRUE;
#endif
    return EB_ErrorNone;
}

void set_cdf_controls(PictureControlSet *pcs, uint8_t update_cdf_level) {
    CdfControls *ctrl = &pcs->cdf_ctrl;
    switch (update_cdf_level) {
    case 0:
        ctrl->update_mv   = 0;
        ctrl->update_se   = 0;
        ctrl->update_coef = 0;
        break;
    case 1:
        ctrl->update_mv   = 1;
        ctrl->update_se   = 1;
        ctrl->update_coef = 1;
        break;
    case 2:
        ctrl->update_mv   = 0;
        ctrl->update_se   = 1;
        ctrl->update_coef = 1;
        break;
    case 3:
        ctrl->update_mv   = 0;
        ctrl->update_se   = 1;
        ctrl->update_coef = 0;
        break;
    default: assert(0); break;
    }

    ctrl->update_mv = pcs->slice_type == I_SLICE ? 0 : ctrl->update_mv;
    ctrl->enabled   = ctrl->update_coef | ctrl->update_mv | ctrl->update_se;
}
/******************************************************
* Derive Mode Decision Config Settings for OQ
Input   : encoder mode and tune
Output  : EncDec Kernel signal(s)
******************************************************/
#if !CLN_REF_AREA
#if FTR_SELECTIVE_DLF
extern uint8_t ref_is_high_intra(PictureControlSet *pcs_ptr, uint8_t *dlf_th);
#endif

#if TUNE_NEW_M11_2
void get_ref_intra_percentage(PictureControlSet *pcs_ptr, uint8_t *iperc){

    if (pcs_ptr->slice_type == I_SLICE) {
        *iperc = 100;
        return;
    }
    *iperc = 0;

    EbReferenceObject *ref_obj_l0 =
            (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_0][0]->object_ptr;
    *iperc = ref_obj_l0->slice_type == I_SLICE ? 100 : ref_obj_l0->intra_coded_area;
    if (pcs_ptr->slice_type == B_SLICE) {
        EbReferenceObject *ref_obj_l1 =
            (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_1][0]->object_ptr;
        *iperc += ref_obj_l1->slice_type == I_SLICE ? 100  : ref_obj_l1->intra_coded_area;
    }
}
#endif
#endif
#if CLN_RTIME_MEM_ALLOC
EbErrorType rtime_alloc_ec_ctx_array(PictureControlSet * pcs_ptr, uint16_t all_sb) {
    EB_MALLOC_ARRAY(pcs_ptr->ec_ctx_array, all_sb);
    return EB_ErrorNone;
}
#endif
#if CLN_REG_PD_SIG_SET_2
uint8_t get_nic_level(EbEncMode enc_mode, uint8_t temporal_layer_index);
#endif
EbErrorType signal_derivation_mode_decision_config_kernel_oq(SequenceControlSet *scs_ptr,
                                                             PictureControlSet * pcs_ptr) {
    UNUSED(scs_ptr);
    EbErrorType return_error = EB_ErrorNone;
#if CLN_WM_SIG
    const EbEncMode enc_mode = pcs_ptr->enc_mode;
    const uint8_t   is_ref =  pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag;
    const uint8_t   is_base = pcs_ptr->parent_pcs_ptr->temporal_layer_index==0;
    const EbInputResolution input_resolution = pcs_ptr->parent_pcs_ptr->input_resolution;
#endif
#if CLN_4X4_SIG
    const EB_SLICE slice_type = pcs_ptr->slice_type;
    PictureParentControlSet *ppcs = pcs_ptr->parent_pcs_ptr;
#endif

    uint8_t update_cdf_level = 0;
    if (pcs_ptr->enc_mode <= ENC_M2)
        update_cdf_level = 1;
#if TUNE_M0_M7_MEGA_FEB
    else if (pcs_ptr->enc_mode <= ENC_M4)
#else
    else if (pcs_ptr->enc_mode <= ENC_M4)
#endif
        update_cdf_level = pcs_ptr->slice_type == I_SLICE
        ? 1
        : (pcs_ptr->temporal_layer_index == 0) ? 1 : 3;
#if TUNE_M7_11
#if TUNE_M7_M8
    else if (pcs_ptr->enc_mode <= ENC_M6)
#else
    else if (pcs_ptr->enc_mode <= ENC_M7)
#endif
#else
    else if (pcs_ptr->enc_mode <= ENC_M6)
#endif

        update_cdf_level = pcs_ptr->slice_type == I_SLICE
        ? 1
        : 3;
#if CLN_UPDATE_CDF
    else if (pcs_ptr->enc_mode <= ENC_M11)
        update_cdf_level = pcs_ptr->slice_type == I_SLICE ? 1 : 0;
#else
#if TUNE_M9_11_3
    else if (pcs_ptr->enc_mode <= ENC_M8)
        update_cdf_level = pcs_ptr->slice_type == I_SLICE ? 1 : 0;
#endif
#if TUNE_SHIFT_PRESETS_DOWN && !TUNE_M9_23_BDR || TUNE_M8_M9_FEB24
#if TUNE_M10_M7
#if TUNE_M10_M9_1 && !TUNE_M10_M3_1
    else if (pcs_ptr->enc_mode <= ENC_M10)
#else
    else if (pcs_ptr->enc_mode <= ENC_M9)
#endif
#else
    else if (pcs_ptr->enc_mode <= ENC_M8)
#endif
#else
#if TUNE_M7_M10_PRESETS
    else if (pcs_ptr->enc_mode <= ENC_M10)
#else
    else if (pcs_ptr->enc_mode <= ENC_M8)
#endif
#endif
#if TUNE_M9_11_3
        update_cdf_level = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE)? 0 : (pcs_ptr->slice_type == I_SLICE ? 1 : 0);
#else
        update_cdf_level = pcs_ptr->slice_type == I_SLICE ? 1 : 0;
#endif
#endif
    else
        update_cdf_level = 0;

    //set the conrols uisng the required level
    set_cdf_controls(pcs_ptr, update_cdf_level);

#if OPT_MEMORY_CDF
    if (pcs_ptr->cdf_ctrl.enabled){
        const uint16_t picture_sb_w = pcs_ptr->parent_pcs_ptr->picture_sb_width;
        const uint16_t picture_sb_h = pcs_ptr->parent_pcs_ptr->picture_sb_height;
        const uint16_t all_sb = picture_sb_w * picture_sb_h;
#if CLN_RTIME_MEM_ALLOC
        rtime_alloc_ec_ctx_array(pcs_ptr, all_sb);
#else
        EB_MALLOC_ARRAY(pcs_ptr->ec_ctx_array, all_sb);
#endif
    }
#endif
    //Filter Intra Mode : 0: OFF  1: ON
    // pic_filter_intra_level specifies whether filter intra would be active
    // for a given picture.

    // pic_filter_intra_level | Settings
    // 0                      | OFF
    // 1                      | ON
    if (scs_ptr->static_config.filter_intra_level == DEFAULT) {
        if (scs_ptr->seq_header.filter_intra_level) {
            if (pcs_ptr->enc_mode <= ENC_M5)
                pcs_ptr->pic_filter_intra_level = 1;
            else
                pcs_ptr->pic_filter_intra_level = 0;
        } else
            pcs_ptr->pic_filter_intra_level = 0;
    } else
        pcs_ptr->pic_filter_intra_level = scs_ptr->static_config.filter_intra_level;
#if OPT9_RATE_ESTIMATION
#if TUNE_MEGA_M9_M4 && !TUNE_M6_MT
    if (pcs_ptr->enc_mode <= ENC_M5)
#else
    if (pcs_ptr->enc_mode <= ENC_M6)
#endif
        pcs_ptr->parent_pcs_ptr->partition_contexts = PARTITION_CONTEXTS;
    else
        pcs_ptr->parent_pcs_ptr->partition_contexts = 4;
#endif
    FrameHeader *frm_hdr             = &pcs_ptr->parent_pcs_ptr->frm_hdr;
    frm_hdr->allow_high_precision_mv = frm_hdr->quantization_params.base_q_idx <
                HIGH_PRECISION_MV_QTHRESH &&
            (scs_ptr->input_resolution <= INPUT_SIZE_480p_RANGE)
        ? 1
        : 0;

#if CLN_WM_SIG

        /*
               Warped Motion
        */
        pcs_ptr->wm_level = 0;
        EbBool enable_wm;
        if (frm_hdr->frame_type == KEY_FRAME || frm_hdr->frame_type == INTRA_ONLY_FRAME ||
            frm_hdr->error_resilient_mode || pcs_ptr->parent_pcs_ptr->frame_superres_enabled)
        {
            pcs_ptr->wm_level = 0;
            enable_wm = 0;
        }
        else {

            if (enc_mode <= ENC_M3) {
                pcs_ptr->wm_level = enable_wm = 1;
            }
            else if (enc_mode <= ENC_M4) {
                enable_wm = is_ref ? EB_TRUE : EB_FALSE;
                pcs_ptr->wm_level = enable_wm;
            }
#if TUNE_M1_M8
            else if (enc_mode <= ENC_M8) {
#else
            else if (enc_mode <= ENC_M9) {
#endif
                enable_wm = is_base ? EB_TRUE : EB_FALSE;
                pcs_ptr->wm_level = enable_wm;
            }
            else if (enc_mode <= ENC_M11) {

                enable_wm = is_base ? EB_TRUE : EB_FALSE;

                if (enable_wm)
                    pcs_ptr->wm_level = (input_resolution <= INPUT_SIZE_360p_RANGE) ? 1 : 2;
                else
                    pcs_ptr->wm_level = 0;

            }
            else if (enc_mode <= ENC_M12) {
                enable_wm = is_base ? EB_TRUE : EB_FALSE;
                if (enable_wm)
                    pcs_ptr->wm_level = 2;
                else
                    pcs_ptr->wm_level = 0;
            }
            else if (enc_mode <= ENC_M13) {
                if (input_resolution <= INPUT_SIZE_720p_RANGE)
                    enable_wm = is_base ? EB_TRUE : EB_FALSE;
                else
                    enable_wm = EB_FALSE;

                if (enable_wm)
                    pcs_ptr->wm_level = 2;
                else
                    pcs_ptr->wm_level = 0;

            }
            else {
                enable_wm = EB_FALSE;
            }
        }

#else




    EbBool enable_wm;
#if TUNE_M3_M6_MEM_OPT
    if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M3)
        enable_wm = EB_TRUE;
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M4) {
        enable_wm = (pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag) ? EB_TRUE : EB_FALSE;
#else
    if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M3) {
        enable_wm = EB_TRUE;
#endif
#if TUNE_SHIFT_PRESETS_DOWN && !FTR_NEW_WM_LVL
    }
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M8) {
#else
#if TUNE_M7_M10_PRESETS
#if TUNE_NEW_M10_M11
#if OPT_TXS_WM
#if CLN_M6_M12_FEATURES
#if TUNE_IMPROVE_M12
    } else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M12) {
#else
    } else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M11) {
#endif
#else
    } else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M7) {
#endif
#else
    } else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M11) {
#endif
#else
    } else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10) {
#endif
#else
    } else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M8) {
#endif
#endif
        enable_wm = (pcs_ptr->parent_pcs_ptr->temporal_layer_index == 0) ? EB_TRUE : EB_FALSE;
#if OPT_TXS_WM
    }
#if TUNE_NEW_M12
#if FTR_M13
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M13) {
#else
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M12) {
#endif
#else
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M11) {
#endif
        if (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_720p_RANGE)
            enable_wm = (pcs_ptr->parent_pcs_ptr->temporal_layer_index == 0) ? EB_TRUE : EB_FALSE;
        else
            enable_wm = EB_FALSE;
#endif
    } else {
        enable_wm = EB_FALSE;
    }
#if FTR_OPT_MPASS_WARP_OFF
#if FTR_OP_TEST
    if (1)
#else
    if (pcs_ptr->parent_pcs_ptr->scs_ptr->rc_stat_gen_pass_mode)
#endif
        enable_wm = EB_FALSE;
#endif

#endif
    if (pcs_ptr->parent_pcs_ptr->scs_ptr->static_config.enable_warped_motion != DEFAULT)
        enable_wm = (EbBool)pcs_ptr->parent_pcs_ptr->scs_ptr->static_config.enable_warped_motion;

    // Note: local warp should be disabled when super-res is ON
    // according to the AV1 spec 5.11.27
    frm_hdr->allow_warped_motion = enable_wm &&
        !(frm_hdr->frame_type == KEY_FRAME || frm_hdr->frame_type == INTRA_ONLY_FRAME) &&
        !frm_hdr->error_resilient_mode && !pcs_ptr->parent_pcs_ptr->frame_superres_enabled;

    frm_hdr->is_motion_mode_switchable = frm_hdr->allow_warped_motion;

    // pic_obmc_level - pic_obmc_level is used to define md_pic_obmc_level.
    // The latter determines the OBMC settings in the function set_obmc_controls.
    // Please check the definitions of the flags/variables in the function
    // set_obmc_controls corresponding to the pic_obmc_level settings.
#if FIX_REMOVE_PD1
    //  pic_obmc_level  | Default Encoder Settings
    //         0        | OFF subject to possible constraints
    //       > 1        | Faster level subject to possible constraints
#else
    //  pic_obmc_level  |              Default Encoder Settings             |     Command Line Settings
    //         0        | OFF subject to possible constraints               | OFF everywhere in encoder
    //         1        | ON subject to possible constraints                | Fully ON in PD_PASS_2
    //         2        | Faster level subject to possible constraints      | Level 2 everywhere in PD_PASS_2
    //         3        | Even faster level subject to possible constraints | Level 3 everywhere in PD_PASS_3
#endif
    if (scs_ptr->static_config.obmc_level == DEFAULT) {
        if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M3)
            pcs_ptr->parent_pcs_ptr->pic_obmc_level = 1;
#if TUNE_M5_M6
#if TUNE_M7_SLOWDOWN && !TUNE_M1_M8
        else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M7)
#else
        else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M6)
#endif
#else
        else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M5)
#endif
            pcs_ptr->parent_pcs_ptr->pic_obmc_level = 2;
#if  !TUNE_MEGA_M9_M4
        else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M7)
            pcs_ptr->parent_pcs_ptr->pic_obmc_level = pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag ? 2 : 0;
#endif
        else
            pcs_ptr->parent_pcs_ptr->pic_obmc_level = 0;

    } else
        pcs_ptr->parent_pcs_ptr->pic_obmc_level = scs_ptr->static_config.obmc_level;

    // Switchable Motion Mode
    frm_hdr->is_motion_mode_switchable = frm_hdr->is_motion_mode_switchable ||
        pcs_ptr->parent_pcs_ptr->pic_obmc_level;

    pcs_ptr->parent_pcs_ptr->bypass_cost_table_gen = 0;
#if !CLN_M6_M12_FEATURES
    if(scs_ptr->input_resolution <= INPUT_SIZE_480p_RANGE)
         pcs_ptr->parent_pcs_ptr->bypass_cost_table_gen = 0;
    else if(pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M7)
        pcs_ptr->parent_pcs_ptr->bypass_cost_table_gen = 0;
    //else if(pcs_ptr->slice_type == I_SLICE)
    else if(pcs_ptr->picture_number == 0)
        pcs_ptr->parent_pcs_ptr->bypass_cost_table_gen = 0;
    else
        pcs_ptr->parent_pcs_ptr->bypass_cost_table_gen = 1;
#endif
#if FTR_SELECTIVE_DLF
#if TUNE_NEW_M11_2
#if TUNE_M10_M0
#if TUNE_M9_M11_OPTIMIZED_SUPER4KL
#if TUNE_M11_SLOWDOWN
#if CLN_REF_AREA
#if TUNE_IMPROVE_M11_M10
    uint8_t use_selective_dlf_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10 ? (uint8_t)~0 : ((pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? 60 : (uint8_t)~0);
#else
    uint8_t use_selective_dlf_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M11 ? (uint8_t)~0 : ((pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? 60 : (uint8_t)~0);
#endif
#else
    uint8_t use_selective_dlf_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M11 ? (uint8_t)~0 : ((pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? 120 : (uint8_t)~0);
#endif
#else
    uint8_t use_selective_dlf_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10 ? (uint8_t)~0 : ((pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? 120 : (uint8_t)~0);
#endif
#else
    uint8_t use_selective_dlf_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10 ? (uint8_t)~0 : 120;
#endif
#else
    uint8_t use_selective_dlf_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M9 ? (uint8_t)~0 : (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10 ? 80 : 120);
#endif
    if (use_selective_dlf_th != (uint8_t)~0){
        if (pcs_ptr->parent_pcs_ptr->temporal_layer_index) {
#if CLN_REF_AREA
            uint8_t dlf_th = pcs_ptr->ref_intra_percentage;
#else
            uint8_t dlf_th = 100;
            ref_is_high_intra(pcs_ptr, &dlf_th);
#endif
            if (dlf_th < use_selective_dlf_th)
                pcs_ptr->parent_pcs_ptr->dlf_ctrls.enabled = 0;
        }
    }
#else
    // Update DLF
    if (pcs_ptr->parent_pcs_ptr->enc_mode > ENC_M9){
        if (pcs_ptr->parent_pcs_ptr->temporal_layer_index) {
            uint8_t dlf_th = 100;
            ref_is_high_intra(pcs_ptr, &dlf_th);
            if (dlf_th < 80)
                pcs_ptr->parent_pcs_ptr->dlf_ctrls.enabled = 0;
        }
    }
#endif
#endif
#if !CLN_USE_SELECTIVE_MFMV_TH
#if FTR_SELECTIVE_MFMV
#if TUNE_M8_M10_4K_SUPER
#if TUNE_M9_SLOWDOWN
#if CLN_REF_AREA
    uint8_t use_selective_mfmv_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M9 ? (uint8_t)~0 : 35; // This is a % [0-100]
#else
    uint8_t use_selective_mfmv_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M9 ? (uint8_t)~0 : 70; // This is a % [0-100]
#endif
#else
    uint8_t use_selective_mfmv_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M8 ? (uint8_t)~0 : ((pcs_ptr->parent_pcs_ptr->enc_mode == ENC_M9) && (pcs_ptr->parent_pcs_ptr->input_resolution > INPUT_SIZE_1080p_RANGE)) ? (uint8_t)~0 : 70; // This is a % [0-100]
#endif
#else
    uint8_t use_selective_mfmv_th = pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M8 ? (uint8_t)~0 : 70; // This is a % [0-100]
#endif
    if (use_selective_mfmv_th != (uint8_t)~0) {
        if (pcs_ptr->slice_type == I_SLICE)
            pcs_ptr->parent_pcs_ptr->frm_hdr.use_ref_frame_mvs = 0;
        else
        {
#if CLN_REF_AREA
            uint8_t rmv_th = pcs_ptr->ref_intra_percentage;
#else
            uint8_t rmv_th = 100;
            ref_is_high_intra(pcs_ptr, &rmv_th);
#endif
            if (rmv_th > use_selective_mfmv_th)
                pcs_ptr->parent_pcs_ptr->frm_hdr.use_ref_frame_mvs = 0;
        }
    }
#endif
#endif
#if CLN_RATE_EST_CTRLS
#if CLN_FEAT_LEVEL
    if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10)
#else
    if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M9)
#endif
        pcs_ptr->approx_inter_rate = 0;
    else
        pcs_ptr->approx_inter_rate = 1;
#else
#if  FTR_SIMPLIFIED_MV_COST
#if FTR_LOW_AC_COST_EST
#if OPT_PD0_TXT
#if TUNE_M9_SLOW
#if TUNE_M10_M0 && !TUNE_M9_M10
    if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10)
#else
    if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M9)
#endif
#else
    if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M8)
#endif
#else
    if (enc_mode <= ENC_M9)
#endif
        pcs_ptr->use_low_precision_cost_estimation = 0;
#if OPT_COEFF_BIT_EST
#if TUNE_M11_SLOWDOWN
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M11)
#else
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10)
#endif
        pcs_ptr->use_low_precision_cost_estimation = 2;
    else
#if OPT_TX_SHORTS
#if TUNE_M9_11_3
        pcs_ptr->use_low_precision_cost_estimation = scs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE ? (pcs_ptr->parent_pcs_ptr->slice_type == I_SLICE ? 2 : 3) : 2;
#else
        pcs_ptr->use_low_precision_cost_estimation = pcs_ptr->parent_pcs_ptr->slice_type == I_SLICE ? 2 : 3;
#endif
#else
        pcs_ptr->use_low_precision_cost_estimation = 3;
#endif
#else
    else
#if FTR_PD0_OPT
        pcs_ptr->use_low_precision_cost_estimation = 2;
#else
        pcs_ptr->use_low_precision_cost_estimation = 1;
#endif
#endif
#endif
#endif
#endif
#if SS_FIX_MOVE_SKIP_INTRA_PIC
    if (pcs_ptr->slice_type == I_SLICE)
        pcs_ptr->skip_intra = 0;
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M8)
        pcs_ptr->skip_intra = 0;
#if CLN_M10_M12_DIFFS
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M11)
#else
    else if (pcs_ptr->parent_pcs_ptr->enc_mode <= ENC_M10)
#endif
#if CLN_REF_AREA
        pcs_ptr->skip_intra = pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag || pcs_ptr->ref_intra_percentage > 50 ? 0 : 1;
    else
        pcs_ptr->skip_intra = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? (pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag ? 0 : 1) : (pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag || pcs_ptr->ref_intra_percentage > 50 ? 0 : 1);
#else
        pcs_ptr->skip_intra = pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag || pcs_ptr->intra_percentage > 100 ? 0 : 1;
    else
        pcs_ptr->skip_intra = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? (pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag ? 0 : 1) : (pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag || pcs_ptr->intra_percentage > 100 ? 0 : 1);
#endif
#endif

#if CLN_REG_PD_SIG_SET_0
    // Set the level for the candidate(s) reduction feature
    pcs_ptr->cand_reduction_level = 0;
    if (pcs_ptr->slice_type == I_SLICE)
        pcs_ptr->cand_reduction_level = 0;
#if TUNE_M7_M9_ULTRA
    else if (enc_mode <= ENC_M6)
#else
    else if (enc_mode <= ENC_M7)
#endif
        pcs_ptr->cand_reduction_level = 0;
    else if (enc_mode <= ENC_M9)
        pcs_ptr->cand_reduction_level = 1;
    else
        pcs_ptr->cand_reduction_level = 2;

    if (pcs_ptr->parent_pcs_ptr->scs_ptr->rc_stat_gen_pass_mode)
        pcs_ptr->cand_reduction_level = 5;

    // Set the level for the txt search
    pcs_ptr->txt_level = 0;
    if (enc_mode <= ENC_M2)
        pcs_ptr->txt_level = 1;
    else if (enc_mode <= ENC_M4)
        pcs_ptr->txt_level = (pcs_ptr->temporal_layer_index == 0) ? 1 : 3;
    else if (enc_mode <= ENC_M8)
        pcs_ptr->txt_level = 5;
#if TUNE_M10_M12_ULTRA
    else if (enc_mode <= ENC_M11) {
#else
    else if (enc_mode <= ENC_M10) {
#endif
#if CLN_RES_TXT
        pcs_ptr->txt_level = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? ((pcs_ptr->temporal_layer_index == 0) ? 6 : 8) : ((pcs_ptr->temporal_layer_index == 0) ? 6 : 9);
        if (pcs_ptr->ref_intra_percentage < 85 && pcs_ptr->temporal_layer_index && pcs_ptr->parent_pcs_ptr->input_resolution > INPUT_SIZE_1080p_RANGE && !pcs_ptr->parent_pcs_ptr->sc_class1) {
            pcs_ptr->txt_level = 0;
        }
#else
        pcs_ptr->txt_level = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_480p_RANGE) ? ((pcs_ptr->temporal_layer_index == 0) ? 6 : 8) : ((pcs_ptr->temporal_layer_index == 0) ? 6 : 9);
        if (pcs_ptr->ref_intra_percentage < 85 && pcs_ptr->temporal_layer_index && pcs_ptr->parent_pcs_ptr->input_resolution > INPUT_SIZE_480p_RANGE && !pcs_ptr->parent_pcs_ptr->sc_class1) {
            pcs_ptr->txt_level = 0;
        }
#endif
    }
    else if (enc_mode <= ENC_M13) {
#if CLN_RES_TXT
        pcs_ptr->txt_level = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? ((pcs_ptr->temporal_layer_index == 0) ? 6 : 9) : ((pcs_ptr->temporal_layer_index == 0) ? 7 : 10);
        if (pcs_ptr->ref_intra_percentage < 85 && pcs_ptr->temporal_layer_index && pcs_ptr->parent_pcs_ptr->input_resolution > INPUT_SIZE_1080p_RANGE && !pcs_ptr->parent_pcs_ptr->sc_class1) {
            pcs_ptr->txt_level = 0;
        }
#else
        pcs_ptr->txt_level = (pcs_ptr->temporal_layer_index == 0) ? 7 : 10;
        if (pcs_ptr->ref_intra_percentage < 85 && pcs_ptr->temporal_layer_index && pcs_ptr->parent_pcs_ptr->input_resolution > INPUT_SIZE_480p_RANGE && !pcs_ptr->parent_pcs_ptr->sc_class1) {
            pcs_ptr->txt_level = 0;
        }
#endif
    }
    else
        pcs_ptr->txt_level = 0;

    // Set the level for the txt shortcut feature
    pcs_ptr->tx_shortcut_level = 0;
    if (enc_mode <= ENC_M5)
        pcs_ptr->tx_shortcut_level = 0;
    else if (enc_mode <= ENC_M11)
        pcs_ptr->tx_shortcut_level = pcs_ptr->slice_type == I_SLICE ? 0 : 1;
    else if (enc_mode <= ENC_M12)
        pcs_ptr->tx_shortcut_level = pcs_ptr->slice_type == I_SLICE ? 0 : 4;
    else
        pcs_ptr->tx_shortcut_level = pcs_ptr->slice_type == I_SLICE ? 0 : 5;

    // Set the level the interpolation search
    pcs_ptr->interpolation_search_level = 0;

    if (enc_mode <= ENC_MR)
        pcs_ptr->interpolation_search_level = 2;
    else if (enc_mode <= ENC_M6)
        pcs_ptr->interpolation_search_level = 4;
#if CLN_RES_IFS
    else if (enc_mode <= ENC_M7) {
        pcs_ptr->interpolation_search_level = 4;
        const uint8_t th[INPUT_SIZE_COUNT] = { 100,100,100,55,50,45,40 };
        uint8_t skip_area = pcs_ptr->parent_pcs_ptr->temporal_layer_index == 0 ? 0 : pcs_ptr->ref_skip_percentage;
        if (skip_area > th[pcs_ptr->parent_pcs_ptr->input_resolution])
            pcs_ptr->interpolation_search_level = 0;

    }
    else {
        pcs_ptr->interpolation_search_level = 4;
        const uint8_t th[INPUT_SIZE_COUNT] = { 100,100,85,50,40,30,20 };
        uint8_t skip_area = pcs_ptr->parent_pcs_ptr->temporal_layer_index == 0 ? 0 : pcs_ptr->ref_skip_percentage;
        if (skip_area > th[pcs_ptr->parent_pcs_ptr->input_resolution])
            pcs_ptr->interpolation_search_level = 0;
    }
#else
    else if (enc_mode <= ENC_M7 /* only for 4K*/) {
        pcs_ptr->interpolation_search_level = 4;
        if (pcs_ptr->parent_pcs_ptr->input_resolution > INPUT_SIZE_1080p_RANGE) {
            const uint8_t th[INPUT_SIZE_COUNT] = { 100,85,85,55,50,45,40 };
            uint8_t skip_area = pcs_ptr->parent_pcs_ptr->temporal_layer_index == 0 ? 0 : pcs_ptr->ref_skip_percentage;
            if (skip_area > th[pcs_ptr->parent_pcs_ptr->input_resolution])
                pcs_ptr->interpolation_search_level = 0;
        }
    }
    else {
        pcs_ptr->interpolation_search_level = 4;
        const uint8_t th[INPUT_SIZE_COUNT] = { 100,85,85,55,50,45,40 };
        uint8_t skip_area = pcs_ptr->parent_pcs_ptr->temporal_layer_index == 0 ? 0 : pcs_ptr->ref_skip_percentage;
        if (skip_area > th[pcs_ptr->parent_pcs_ptr->input_resolution])
            pcs_ptr->interpolation_search_level = 0;
    }
#endif
    // Set the level for the chroma path
    pcs_ptr->chroma_level = 0;
    if (scs_ptr->static_config.set_chroma_mode == DEFAULT) {
        if (enc_mode <= ENC_MRS)
            pcs_ptr->chroma_level = 1;
        else if (enc_mode <= ENC_M1)
            pcs_ptr->chroma_level = 2;
        else if (enc_mode <= ENC_M5)
            pcs_ptr->chroma_level = 3;
        else
            pcs_ptr->chroma_level = 4;
    }
    else // use specified level
        pcs_ptr->chroma_level = scs_ptr->static_config.set_chroma_mode;

    // Set the level for cfl
    pcs_ptr->cfl_level = 0;
    if (pcs_ptr->parent_pcs_ptr->sc_class1) {
        if (enc_mode <= ENC_M6)
            pcs_ptr->cfl_level = 1;
        else
            pcs_ptr->cfl_level = (pcs_ptr->temporal_layer_index == 0) ? 2 : 0;
    } else if (enc_mode <= ENC_M5)
        pcs_ptr->cfl_level = 1;
#if TUNE_M10_M12_ULTRA
    else if (enc_mode <= ENC_M11)
#else
    else if (enc_mode <= ENC_M10)
#endif
        pcs_ptr->cfl_level = (pcs_ptr->temporal_layer_index == 0) ? 2 : 0;
    else if (enc_mode <= ENC_M12)
        pcs_ptr->cfl_level = (pcs_ptr->slice_type == I_SLICE) ? 2 : 0;
    else
        pcs_ptr->cfl_level = 0;
#endif

#if CLN_REG_PD_SIG_SET_1
    // Set the level for new/nearest/near injection
    if (scs_ptr->static_config.new_nearest_comb_inject == DEFAULT)
        if (enc_mode <= ENC_M0)
            pcs_ptr->new_nearest_near_comb_injection = 1;
        else
            pcs_ptr->new_nearest_near_comb_injection = 0;
    else
        pcs_ptr->new_nearest_near_comb_injection = scs_ptr->static_config.new_nearest_comb_inject;

    // Set the level for unipred3x3 injection
    if (enc_mode <= ENC_M0)
        pcs_ptr->unipred3x3_injection = 1;
    else if (enc_mode <= ENC_M1)
        pcs_ptr->unipred3x3_injection = 2;
    else
        pcs_ptr->unipred3x3_injection = 0;

    // Set the level for bipred3x3 injection
     if (scs_ptr->static_config.bipred_3x3_inject == DEFAULT) {
        if (enc_mode <= ENC_M1)
            pcs_ptr->bipred3x3_injection = 1;
        else if (enc_mode <= ENC_M5)
            pcs_ptr->bipred3x3_injection = 2;
        else
            pcs_ptr->bipred3x3_injection = 0;
    }
    else {
         pcs_ptr->bipred3x3_injection = scs_ptr->static_config.bipred_3x3_inject;
    }

     // Set the level for inter-inter compound
    if (scs_ptr->compound_mode) {
        if (scs_ptr->static_config.compound_level == DEFAULT) {
            if (enc_mode <= ENC_MR)
                pcs_ptr->inter_compound_mode = 1;
            else if (enc_mode <= ENC_M2)
                pcs_ptr->inter_compound_mode = 2;
            else if (enc_mode <= ENC_M3)
                pcs_ptr->inter_compound_mode = 3;
            else
                pcs_ptr->inter_compound_mode = 0;
        }
        else {
            pcs_ptr->inter_compound_mode = scs_ptr->static_config.compound_level;
        }
    }
    else {
        pcs_ptr->inter_compound_mode = 0;
    }

    // Set the level for the distance-based red pruning
    if (pcs_ptr->parent_pcs_ptr->ref_list0_count_try > 1 || pcs_ptr->parent_pcs_ptr->ref_list1_count_try > 1) {
         if (enc_mode <= ENC_MR)
             pcs_ptr->dist_based_ref_pruning = 1;
        else if (enc_mode <= ENC_M0)
             pcs_ptr->dist_based_ref_pruning = (pcs_ptr->temporal_layer_index == 0) ? 1 : 2;
        else
             pcs_ptr->dist_based_ref_pruning = (pcs_ptr->temporal_layer_index == 0) ? 2 : 4;
    }
    else {
        pcs_ptr->dist_based_ref_pruning = 0;
    }

    // Set the level the spatial sse @ full-loop
    pcs_ptr->spatial_sse_full_loop_level = 0;
     if (scs_ptr->static_config.spatial_sse_full_loop_level == DEFAULT)
        if (pcs_ptr->parent_pcs_ptr->sc_class1)
            pcs_ptr->spatial_sse_full_loop_level = 1;
        else if (enc_mode <= ENC_M9)
            pcs_ptr->spatial_sse_full_loop_level = 1;
        else
            pcs_ptr->spatial_sse_full_loop_level = 0;
    else
         pcs_ptr->spatial_sse_full_loop_level = scs_ptr->static_config.spatial_sse_full_loop_level;
#endif
#if CLN_REG_PD_SIG_SET_2
     // Set the level for coeff-based NSQ accuracy reduction
     pcs_ptr->parent_sq_coeff_area_based_cycles_reduction_level = 0;
     if (enc_mode <= ENC_MRS)
         pcs_ptr->parent_sq_coeff_area_based_cycles_reduction_level = 0;
     else if (enc_mode <= ENC_MR)
         pcs_ptr->parent_sq_coeff_area_based_cycles_reduction_level = pcs_ptr->slice_type == I_SLICE ? 0 : 1;
     else if (enc_mode <= ENC_M1)
         pcs_ptr->parent_sq_coeff_area_based_cycles_reduction_level = pcs_ptr->slice_type == I_SLICE ? 0 : 2;
     else if (enc_mode <= ENC_M2)
         pcs_ptr->parent_sq_coeff_area_based_cycles_reduction_level = pcs_ptr->slice_type == I_SLICE ? 0
         : (pcs_ptr->temporal_layer_index == 0) ? 2 : pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag ? 4 : 7;
     else
         pcs_ptr->parent_sq_coeff_area_based_cycles_reduction_level = pcs_ptr->slice_type == I_SLICE ? 5 : pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag ? 6 : 7;

     // Weighting (expressed as a percentage) applied to
     // square shape costs for determining if a and b
     // shapes should be skipped. Namely:
     // skip HA, HB, and H4 if h_cost > (weighted sq_cost)
     // skip VA, VB, and V4 if v_cost > (weighted sq_cost)
     if (enc_mode <= ENC_MRS)
         pcs_ptr->sq_weight = (uint32_t)~0;
     else
         if (enc_mode <= ENC_M0)
             pcs_ptr->sq_weight = 105;
         else
             pcs_ptr->sq_weight = 95;

     // max_part0_to_part1_dev is used to:
     // (1) skip the H_Path if the deviation between the Parent-SQ src-to-recon distortion of (1st quadrant + 2nd quadrant) and the Parent-SQ src-to-recon distortion of (3rd quadrant + 4th quadrant) is less than TH,
     // (2) skip the V_Path if the deviation between the Parent-SQ src-to-recon distortion of (1st quadrant + 3rd quadrant) and the Parent-SQ src-to-recon distortion of (2nd quadrant + 4th quadrant) is less than TH.
     if (enc_mode <= ENC_M3)
         pcs_ptr->max_part0_to_part1_dev = 0;
     else
         pcs_ptr->max_part0_to_part1_dev = 100;

     // Set the level for enable_inter_intra
     // Block level switch, has to follow the picture level
     // inter intra pred                      Settings
     // 0                                     OFF
     // 1                                     FULL
     // 2                                     FAST 1 : Do not inject for unipred3x3 or PME inter candidates
     // 3                                     FAST 2 : Level 1 + do not inject for non-closest ref frames or ref frames with high distortion
     if (pcs_ptr->parent_pcs_ptr->slice_type != I_SLICE && scs_ptr->seq_header.enable_interintra_compound) {
          if (enc_mode <= ENC_M2)
              pcs_ptr->md_inter_intra_level = 1;
         else
              pcs_ptr->md_inter_intra_level = 0;
     }
     else
         pcs_ptr->md_inter_intra_level = 0;

     // Set the level for the tx search
     pcs_ptr->txs_level = 0;
     if (pcs_ptr->parent_pcs_ptr->tx_size_search_mode == 0)
         pcs_ptr->txs_level = 0;
     else if (enc_mode <= ENC_MRS)
         pcs_ptr->txs_level = 1;
     else if (enc_mode <= ENC_MR)
         pcs_ptr->txs_level = 2;
     else if (enc_mode <= ENC_M2)
         pcs_ptr->txs_level = (pcs_ptr->temporal_layer_index == 0) ? 2 : 3;
#if TUNE_M10_M12_ULTRA
     else if (enc_mode <= ENC_M10)
#else
     else if (enc_mode <= ENC_M11)
#endif
         pcs_ptr->txs_level = 3;
     else if (enc_mode <= ENC_M13)
         pcs_ptr->txs_level = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_240p_RANGE) ? 5 : (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? 4 : 1;
     else
         pcs_ptr->txs_level = 0;

     // Set the level for nic
     pcs_ptr->nic_level = get_nic_level(enc_mode, pcs_ptr->temporal_layer_index);

     // Set the level for SQ me-search
     if (enc_mode <= ENC_M3)
         pcs_ptr->md_sq_mv_search_level = 1;
     else
         pcs_ptr->md_sq_mv_search_level = 0;

     // Set the level for NSQ me-search
     if (enc_mode <= ENC_MRS)
         pcs_ptr->md_nsq_mv_search_level = 2;
     else
         pcs_ptr->md_nsq_mv_search_level = 4;

     // Set the level for PME search
     if (enc_mode <= ENC_M0)
         pcs_ptr->md_pme_level = (pcs_ptr->parent_pcs_ptr->input_resolution <= INPUT_SIZE_1080p_RANGE) ? 2 : 1;
     else if (enc_mode <= ENC_M5)
         pcs_ptr->md_pme_level = 3;
     else if (enc_mode <= ENC_M6)
         pcs_ptr->md_pme_level = 4;
#if TUNE_M10_M12_ULTRA
     else if (enc_mode <= ENC_M12)
#else
     else if (enc_mode <= ENC_M11)
#endif
         pcs_ptr->md_pme_level = 6;
#if !TUNE_M10_M12_ULTRA
     else if (enc_mode <= ENC_M12)
         pcs_ptr->md_pme_level = 10;
#endif
     else
         pcs_ptr->md_pme_level = 0;

     // Set the level for mds0
     pcs_ptr->mds0_level = 0;
     if (enc_mode <= ENC_M11)
         pcs_ptr->mds0_level = 2;
     else
         pcs_ptr->mds0_level = pcs_ptr->slice_type == I_SLICE ? 2 : 4;
#endif
#if CLN_4X4_SIG
    /*
       disallow_4x4
    */
    pcs_ptr->pic_disallow_4x4 = get_disallow_4x4(enc_mode, slice_type);
#endif
#if CLN_BYP_ED_SIG
    /*
       Bypassing EncDec
    */

    // TODO: Bypassing EncDec doesn't work if HVA_HVB_HV4 are enabled (for all bit depths; causes non-conformant bitstreams),
    // or if NSQ is enabled for 10bit content (causes r2r).
    // TODO: This signal can only be modified per picture right now, not per SB.  Per SB requires
    // neighbour array updates at EncDec for all SBs, that are currently skipped if EncDec is bypassed.

    if (ppcs->disallow_HVA_HVB_HV4 &&
        (scs_ptr->static_config.encoder_bit_depth == EB_8BIT || ppcs->disallow_nsq) &&
        !ppcs->frm_hdr.segmentation_params.segmentation_enabled) {

        pcs_ptr->pic_bypass_encdec = get_bypass_encdec(enc_mode, ppcs->hbd_mode_decision, scs_ptr->static_config.encoder_bit_depth);
    }
    else
        pcs_ptr->pic_bypass_encdec = 0;
#endif
#if CLN_PD0_SIG
    /*
        set pd0_level
    */
    if (enc_mode <= ENC_M4)
        pcs_ptr->pic_pd0_level = REGULAR_PD0;
    else if (enc_mode <= ENC_M7)
        pcs_ptr->pic_pd0_level = LIGHT_PD0_LVL1;
#if TUNE_M10_M12_ULTRA
    else if (enc_mode <= ENC_M10)
#else
    else if (enc_mode <= ENC_M9)
#endif
        pcs_ptr->pic_pd0_level = LIGHT_PD0_LVL2;
    else
#if CLN_RES_CHECKS
        pcs_ptr->pic_pd0_level = is_base ? LIGHT_PD0_LVL4 : VERY_LIGHT_PD0;
#else
        pcs_ptr->pic_pd0_level = input_resolution <= INPUT_SIZE_1080p_RANGE ? (is_base ? LIGHT_PD0_LVL4 : VERY_LIGHT_PD0) : LIGHT_PD0_LVL2;
#endif
#endif
#if CLN_SKIP_PD0_SIG
#if TUNE_M9_M13
    if (pcs_ptr->parent_pcs_ptr->sc_class1)
        pcs_ptr->pic_skip_pd0 = 0;
    else if (enc_mode <= ENC_M12)
#else
    if (enc_mode <= ENC_M12)
#endif
        pcs_ptr->pic_skip_pd0 = 0;
    else
        pcs_ptr->pic_skip_pd0 = 1;
#endif
#if CLN_DISALLOW_BELOW_16X16_SIG
#if CLN_RES_DISALLOW_B16
    pcs_ptr->pic_disallow_below_16x16 = get_disallow_below_16x16_picture_level(enc_mode, input_resolution, slice_type, ppcs->sc_class1, pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag, pcs_ptr->temporal_layer_index);
#else
    pcs_ptr->pic_disallow_below_16x16 = get_disallow_below_16x16_picture_level(enc_mode, input_resolution, slice_type, ppcs->sc_class1);
#endif
#endif
#if CLN_DEPTH_REMOVAL_SIG
    if (scs_ptr->static_config.super_block_size == 64) {
        if (slice_type == I_SLICE) {
            if (ppcs->sc_class1)
                pcs_ptr->pic_depth_removal_level = 0;
            else if (enc_mode <= ENC_M12)
                pcs_ptr->pic_depth_removal_level = 0;
            else
                pcs_ptr->pic_depth_removal_level = 1;
        }
        else {
            // Set depth_removal_level_controls
#if TUNE_SC_SPACING
            if (pcs_ptr->parent_pcs_ptr->sc_class1) {
                if (enc_mode <= ENC_M8)
                    pcs_ptr->pic_depth_removal_level = 0;
                else if (enc_mode <= ENC_M10) {
                    if (input_resolution <= INPUT_SIZE_360p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 0 : 2;
                    else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 0 : 3;
                    else if (input_resolution <= INPUT_SIZE_1080p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 0 : 6;
                    else
                        pcs_ptr->pic_depth_removal_level = is_base ? 0 : 11;
                }
                else if (enc_mode <= ENC_M12) {
                    if (input_resolution <= INPUT_SIZE_360p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 1 : 2;
                    else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 1 : 3;
                    else if (input_resolution <= INPUT_SIZE_1080p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 4 : 6;
                    else
                        pcs_ptr->pic_depth_removal_level = is_base ? 5 : 11;
                }
                else {
                    if (input_resolution <= INPUT_SIZE_360p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 4 : 8;
                    else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 5 : 11;
                    else if (input_resolution <= INPUT_SIZE_1080p_RANGE)
                        pcs_ptr->pic_depth_removal_level = is_base ? 5 : 14;
                    else
                        pcs_ptr->pic_depth_removal_level = is_base ? 5 : 11;

                }
            }
#else
            if (pcs_ptr->parent_pcs_ptr->sc_class1)
                pcs_ptr->pic_depth_removal_level = 0;
#endif
            else if (enc_mode <= ENC_M2)
                pcs_ptr->pic_depth_removal_level = 0;
            else if (enc_mode <= ENC_M5) {
                if (input_resolution <= INPUT_SIZE_480p_RANGE)
                    pcs_ptr->pic_depth_removal_level = 0;
                else
                    pcs_ptr->pic_depth_removal_level = 2;
            }
#if TUNE_M7_M9_ULTRA
            else if (enc_mode <= ENC_M7) {
#else
            else if (enc_mode <= ENC_M6) {
#endif
                if (input_resolution <= INPUT_SIZE_360p_RANGE)
                    pcs_ptr->pic_depth_removal_level = 1;
                else
                    pcs_ptr->pic_depth_removal_level = is_base ? 1 : 2;
            }
#if CLN_RES_DEPTH_REMOVAL
#if !TUNE_M7_M9_ULTRA
            else if (enc_mode <= ENC_M7) {
                pcs_ptr->pic_depth_removal_level = is_base ? 2 : 3;
            }
#endif
            else if (enc_mode <= ENC_M9) {
                if (input_resolution <= INPUT_SIZE_360p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 2 : 3;
                else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 2 : 4;
                else
                    pcs_ptr->pic_depth_removal_level = is_base ? 2 : 5;
            }
#if TUNE_M10_M12_ULTRA
            else if (enc_mode <= ENC_M11) {
#else
            else if (enc_mode <= ENC_M10) {
#endif
                if (input_resolution <= INPUT_SIZE_360p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 2 : 4;
                else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 2 : 5;
                else if (input_resolution <= INPUT_SIZE_1080p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 2 : 6;
                else
                    pcs_ptr->pic_depth_removal_level = is_base ? 3 : 8;
            }
#if !TUNE_M10_M12_ULTRA
            else if (enc_mode <= ENC_M11) {
                if (input_resolution <= INPUT_SIZE_360p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 3 : 6;
                else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 3 : 7;
                else if (input_resolution <= INPUT_SIZE_1080p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 3 : 8;
                else
                    pcs_ptr->pic_depth_removal_level = is_base ? 4 : 9;
            }
#endif
#else
            else if (enc_mode <= ENC_M11) {
                if (input_resolution <= INPUT_SIZE_360p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 1 : 2;
                else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 1 : 3;
                else if (input_resolution <= INPUT_SIZE_1080p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 4 : 6;
                else
                    pcs_ptr->pic_depth_removal_level = is_base ? 5 : 11;
            }
#endif
            else {
                if (input_resolution <= INPUT_SIZE_360p_RANGE)
#if CLN_RES_DEPTH_REMOVAL
                    pcs_ptr->pic_depth_removal_level = is_base ? 3 : 7;
#else
                    pcs_ptr->pic_depth_removal_level = is_base ? 4 : 8;
#endif
                else if (input_resolution <= INPUT_SIZE_480p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 5 : 11;
#if CLN_RES_CHECKS
                else
                    pcs_ptr->pic_depth_removal_level = is_base ? 5 : 14;
#else
                else if (input_resolution <= INPUT_SIZE_1080p_RANGE)
                    pcs_ptr->pic_depth_removal_level = is_base ? 5 : 14;
                else
                    pcs_ptr->pic_depth_removal_level = is_base ? 5 : 11;
#endif

            }
        }
    }
#endif
#if CLN_BLOCK_BASED_DEPTH_SIG
#if TUNE_SC_SPACING
    if (pcs_ptr->parent_pcs_ptr->sc_class1) {
        if (enc_mode <= ENC_M6)
            pcs_ptr->pic_block_based_depth_refinement_level = 0;
        else if (enc_mode <= ENC_M9)
            pcs_ptr->pic_block_based_depth_refinement_level = is_base ? 0 : 4;
        else if (enc_mode <= ENC_M10)
            pcs_ptr->pic_block_based_depth_refinement_level = (slice_type == I_SLICE) ? 1 : 4;
        else
            pcs_ptr->pic_block_based_depth_refinement_level = (slice_type == I_SLICE) ? 6 : 11;
    }
#else
    // do not use feature for SC
    if (pcs_ptr->parent_pcs_ptr->sc_class1)
        pcs_ptr->pic_block_based_depth_refinement_level = 0;
#endif
    else if (enc_mode <= ENC_M2)
        pcs_ptr->pic_block_based_depth_refinement_level = 0;
    else if (enc_mode <= ENC_M4)
        pcs_ptr->pic_block_based_depth_refinement_level = is_base ? 0 : 2;
    else if (enc_mode <= ENC_M5)
        pcs_ptr->pic_block_based_depth_refinement_level = is_base ? 1 : 2;

    else if (enc_mode <= ENC_M8)
        pcs_ptr->pic_block_based_depth_refinement_level = is_base ? 2 : 4;
    else
#if CLN_RES_DP
        pcs_ptr->pic_block_based_depth_refinement_level = is_base ? 6 : 10;
#else
        pcs_ptr->pic_block_based_depth_refinement_level = (input_resolution <= INPUT_SIZE_480p_RANGE) ? is_base ? 6 : 10 : (slice_type == I_SLICE) ? 6 : 11;
#endif
    if (scs_ptr->static_config.max_heirachical_level == (EB_MAX_TEMPORAL_LAYERS - 1))
        pcs_ptr->pic_block_based_depth_refinement_level = MAX(0, pcs_ptr->pic_block_based_depth_refinement_level - 1);
#endif
#if CLN_LPD1_LVL_SIG
#if TUNE_SC_SPACING
    if (pcs_ptr->parent_pcs_ptr->sc_class1) {
        if (enc_mode <= ENC_M7)
            pcs_ptr->pic_lpd1_lvl = 0;
        else if (enc_mode <= ENC_M9)
            pcs_ptr->pic_lpd1_lvl = (input_resolution <= INPUT_SIZE_480p_RANGE) ? 0 : (is_ref ? 0 : 1);
        else if (enc_mode <= ENC_M10)
            pcs_ptr->pic_lpd1_lvl = is_ref ? 0 : 2;
        else
            pcs_ptr->pic_lpd1_lvl = is_base ? 0 : 2;
    }
    else if (enc_mode <= ENC_M7)
#else
    if (enc_mode <= ENC_M7)
#endif
        pcs_ptr->pic_lpd1_lvl = 0;
#if CLN_RES_LPD1_BIS
    else if (enc_mode <= ENC_M9)
        pcs_ptr->pic_lpd1_lvl = (input_resolution <= INPUT_SIZE_480p_RANGE) ? (is_base ? 0 : 1) : (is_ref ? 0 : 1);
#else
    else if (enc_mode <= ENC_M9)
        pcs_ptr->pic_lpd1_lvl = (input_resolution <= INPUT_SIZE_480p_RANGE) ? 0 : (is_ref ? 0 : 1);
#endif
    else if (enc_mode <= ENC_M10)
#if TUNE_M10_M12_ULTRA
        pcs_ptr->pic_lpd1_lvl = is_base ? 0 : 2;
#else
        pcs_ptr->pic_lpd1_lvl = is_ref ? 0 : 2;
#endif
#if TUNE_NEW_LPD1_LVL
#if TUNE_M10_M12_ULTRA
    else if (enc_mode <= ENC_M11)
        pcs_ptr->pic_lpd1_lvl = is_base ? 0 : 3;
#endif
    else if (enc_mode <= ENC_M12)
        pcs_ptr->pic_lpd1_lvl = is_base ? 0 : 4;
    else
        pcs_ptr->pic_lpd1_lvl = is_base ? 0 : 5;
#else
    // Possible intermediate level for M11: lpd1_lvl = pcs_ptr->parent_pcs_ptr->temporal_layer_index == 0 ? 0 : 2;
    else if (enc_mode <= ENC_M12)
        pcs_ptr->pic_lpd1_lvl = is_base ? 0 : 3;
    else
        pcs_ptr->pic_lpd1_lvl = is_base ? 0 : 4;
#endif
    // Can only use light-PD1 under the following conditions
    // There is another check before PD1 is called; pred_depth_only is not checked here, because some modes
    // may force pred_depth_only at the light-pd1 detector
    if (pcs_ptr->pic_lpd1_lvl &&
        !(ppcs->hbd_mode_decision == 0 && ppcs->disallow_nsq == EB_TRUE && pcs_ptr->pic_disallow_4x4 == EB_TRUE &&
            scs_ptr->static_config.super_block_size == 64)) {
        pcs_ptr->pic_lpd1_lvl = 0;
    }
#endif
    return return_error;
}

/******************************************************
* Derive Mode Decision Config Settings for first pass
Input   : encoder mode and tune
Output  : EncDec Kernel signal(s)
******************************************************/
EbErrorType first_pass_signal_derivation_mode_decision_config_kernel(PictureControlSet *pcs_ptr);
#if !OPT_INLINE_FUNCS
void        av1_set_ref_frame(MvReferenceFrame *rf, int8_t ref_frame_type);
#endif
static INLINE int get_relative_dist(const OrderHintInfo *oh, int a, int b) {
    if (!oh->enable_order_hint)
        return 0;

    const int bits = oh->order_hint_bits;

    assert(bits >= 1);
    assert(a >= 0 && a < (1 << bits));
    assert(b >= 0 && b < (1 << bits));

    int       diff = a - b;
    const int m    = 1 << (bits - 1);
    diff           = (diff & (m - 1)) - (diff & m);
    return diff;
}

static int get_block_position(Av1Common *cm, int *mi_r, int *mi_c, int blk_row, int blk_col, MV mv,
                              int sign_bias) {
    const int base_blk_row = (blk_row >> 3) << 3;
    const int base_blk_col = (blk_col >> 3) << 3;

    const int row_offset = (mv.row >= 0) ? (mv.row >> (4 + MI_SIZE_LOG2))
                                         : -((-mv.row) >> (4 + MI_SIZE_LOG2));

    const int col_offset = (mv.col >= 0) ? (mv.col >> (4 + MI_SIZE_LOG2))
                                         : -((-mv.col) >> (4 + MI_SIZE_LOG2));

    const int row = (sign_bias == 1) ? blk_row - row_offset : blk_row + row_offset;
    const int col = (sign_bias == 1) ? blk_col - col_offset : blk_col + col_offset;

    if (row < 0 || row >= (cm->mi_rows >> 1) || col < 0 || col >= (cm->mi_cols >> 1))
        return 0;

    if (row < base_blk_row - (MAX_OFFSET_HEIGHT >> 3) ||
        row >= base_blk_row + 8 + (MAX_OFFSET_HEIGHT >> 3) ||
        col < base_blk_col - (MAX_OFFSET_WIDTH >> 3) ||
        col >= base_blk_col + 8 + (MAX_OFFSET_WIDTH >> 3))
        return 0;

    *mi_r = row;
    *mi_c = col;

    return 1;
}

#define MFMV_STACK_SIZE 3

// Note: motion_filed_projection finds motion vectors of current frame's
// reference frame, and projects them to current frame. To make it clear,
// let's call current frame's reference frame as start frame.
// Call Start frame's reference frames as reference frames.
// Call ref_offset as frame distances between start frame and its reference
// frames.
static int motion_field_projection(Av1Common *cm, PictureControlSet *pcs_ptr,
                                   MvReferenceFrame start_frame, int dir) {
    TPL_MV_REF *tpl_mvs_base           = pcs_ptr->tpl_mvs;
    int         ref_offset[REF_FRAMES] = {0};

    uint8_t list_idx0, ref_idx_l0;
    list_idx0  = get_list_idx(start_frame);
    ref_idx_l0 = get_ref_frame_idx(start_frame);
    EbReferenceObject *start_frame_buf =
        (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[list_idx0][ref_idx_l0]->object_ptr;

    if (start_frame_buf == NULL)
        return 0;

    if (start_frame_buf->frame_type == KEY_FRAME || start_frame_buf->frame_type == INTRA_ONLY_FRAME)
        return 0;

    // MFMV is not applied when the reference picture is of a different spatial resolution
    // (described in the AV1 spec section 7.9.2.)
    if (start_frame_buf->mi_rows != cm->mi_rows || start_frame_buf->mi_cols != cm->mi_cols) {
        return 0;
    }

    const int                 start_frame_order_hint        = start_frame_buf->order_hint;
    const unsigned int *const ref_order_hints               = &start_frame_buf->ref_order_hint[0];
    int                       start_to_current_frame_offset = get_relative_dist(
        &pcs_ptr->parent_pcs_ptr->scs_ptr->seq_header.order_hint_info,
        start_frame_order_hint,
        pcs_ptr->parent_pcs_ptr->cur_order_hint);

    for (int i = LAST_FRAME; i <= INTER_REFS_PER_FRAME; ++i)
        ref_offset[i] = get_relative_dist(
            &pcs_ptr->parent_pcs_ptr->scs_ptr->seq_header.order_hint_info,
            start_frame_order_hint,
            ref_order_hints[i - LAST_FRAME]);

    if (dir == 2)
        start_to_current_frame_offset = -start_to_current_frame_offset;

    const MV_REF *const mv_ref_base = start_frame_buf->mvs;
    const int           mvs_rows    = (cm->mi_rows + 1) >> 1;
    const int           mvs_cols    = (cm->mi_cols + 1) >> 1;

    for (int blk_row = 0; blk_row < mvs_rows; ++blk_row) {
        for (int blk_col = 0; blk_col < mvs_cols; ++blk_col) {
            const MV_REF *const mv_ref = &mv_ref_base[blk_row * mvs_cols + blk_col];
            MV                  fwd_mv = mv_ref->mv.as_mv;

            if (mv_ref->ref_frame > INTRA_FRAME) {
                MV        this_mv;
                int       mi_r, mi_c;
                const int ref_frame_offset = ref_offset[mv_ref->ref_frame];

                int pos_valid = abs(ref_frame_offset) <= MAX_FRAME_DISTANCE &&
                    ref_frame_offset > 0 &&
                    abs(start_to_current_frame_offset) <= MAX_FRAME_DISTANCE;

                if (pos_valid) {
                    get_mv_projection(
                        &this_mv, fwd_mv, start_to_current_frame_offset, ref_frame_offset);
                    pos_valid = get_block_position(
                        cm, &mi_r, &mi_c, blk_row, blk_col, this_mv, dir >> 1);
                }

                if (pos_valid) {
                    const int mi_offset = mi_r * (cm->mi_stride >> 1) + mi_c;

                    tpl_mvs_base[mi_offset].mfmv0.as_mv.row  = fwd_mv.row;
                    tpl_mvs_base[mi_offset].mfmv0.as_mv.col  = fwd_mv.col;
                    tpl_mvs_base[mi_offset].ref_frame_offset = ref_frame_offset;
                }
            }
        }
    }

    return 1;
}
static void av1_setup_motion_field(Av1Common *cm, PictureControlSet *pcs_ptr) {
    const OrderHintInfo *const order_hint_info =
        &pcs_ptr->parent_pcs_ptr->scs_ptr->seq_header.order_hint_info;
    memset(pcs_ptr->ref_frame_side, 0, sizeof(pcs_ptr->ref_frame_side));
    if (!order_hint_info->enable_order_hint)
        return;

    TPL_MV_REF *tpl_mvs_base = pcs_ptr->tpl_mvs;
    int         size         = ((cm->mi_rows + MAX_MIB_SIZE) >> 1) * (cm->mi_stride >> 1);

    const int                cur_order_hint = pcs_ptr->parent_pcs_ptr->cur_order_hint;
    const EbReferenceObject *ref_buf[INTER_REFS_PER_FRAME];
    int                      ref_order_hint[INTER_REFS_PER_FRAME];

    for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
        const int ref_idx    = ref_frame - LAST_FRAME;
        int       order_hint = 0;
        uint8_t   list_idx0, ref_idx_l0;
        list_idx0  = get_list_idx(ref_frame);
        ref_idx_l0 = get_ref_frame_idx(ref_frame);
        EbReferenceObject *buf =
            (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[list_idx0][ref_idx_l0]->object_ptr;

        if (buf != NULL)
            order_hint = buf->order_hint;

        ref_buf[ref_idx]        = buf;
        ref_order_hint[ref_idx] = order_hint;

        if (get_relative_dist(order_hint_info, order_hint, cur_order_hint) > 0)
            pcs_ptr->ref_frame_side[ref_frame] = 1;
        else if (order_hint == cur_order_hint)
            pcs_ptr->ref_frame_side[ref_frame] = -1;
    }

    //for a frame based mfmv, we need to keep computing the ref_frame_side regardless mfmv is used or no
    if (!pcs_ptr->parent_pcs_ptr->frm_hdr.use_ref_frame_mvs)
        return;


    for (int idx = 0; idx < size; ++idx) {
        tpl_mvs_base[idx].mfmv0.as_int = INVALID_MV;
        tpl_mvs_base[idx].ref_frame_offset = 0;

    }



    int ref_stamp = MFMV_STACK_SIZE - 1;

    if (ref_buf[LAST_FRAME - LAST_FRAME] != NULL) {
        const int alt_of_lst_order_hint =
            ref_buf[LAST_FRAME - LAST_FRAME]->ref_order_hint[ALTREF_FRAME - LAST_FRAME];
        const int is_lst_overlay = (alt_of_lst_order_hint ==
                                    ref_order_hint[GOLDEN_FRAME - LAST_FRAME]);
        if (!is_lst_overlay)
            motion_field_projection(cm, pcs_ptr, LAST_FRAME, 2);

        --ref_stamp;
    }

    if (get_relative_dist(
            order_hint_info, ref_order_hint[BWDREF_FRAME - LAST_FRAME], cur_order_hint) > 0) {
        if (motion_field_projection(cm, pcs_ptr, BWDREF_FRAME, 0))
            --ref_stamp;
    }

        if (get_relative_dist(
                order_hint_info, ref_order_hint[ALTREF2_FRAME - LAST_FRAME], cur_order_hint) > 0) {
            if (motion_field_projection(cm, pcs_ptr, ALTREF2_FRAME, 0))
                --ref_stamp;
        }

    if (get_relative_dist(
            order_hint_info, ref_order_hint[ALTREF_FRAME - LAST_FRAME], cur_order_hint) > 0 &&
        ref_stamp >= 0)
        if (motion_field_projection(cm, pcs_ptr, ALTREF_FRAME, 0))
            --ref_stamp;

    if (ref_stamp >= 0)
        motion_field_projection(cm, pcs_ptr, LAST2_FRAME, 2);
}
#if OPT_MEMORY_HASH_TABLE
EbErrorType svt_av1_hash_table_create(HashTable *p_hash_table);
#endif
#if CLN_REF_AREA
// intra_perc will be set to the % of intra area in two nearest ref frames
void get_ref_intra_percentage(PictureControlSet *pcs_ptr, uint8_t *intra_perc) {

    assert(intra_perc != NULL);
    if (pcs_ptr->slice_type == I_SLICE) {
        *intra_perc = 100;
        return;
    }

    uint8_t iperc = 0;

    EbReferenceObject *ref_obj_l0 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_0][0]->object_ptr;
    iperc = ref_obj_l0->intra_coded_area;
    if (pcs_ptr->slice_type == B_SLICE) {
        EbReferenceObject *ref_obj_l1 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_1][0]->object_ptr;
        iperc += ref_obj_l1->intra_coded_area;

        // if have two frames, divide the iperc by 2 to get the avg skip area
        iperc >>= 1;
    }

    *intra_perc = iperc;
}

// skip_area will be set to the % of skipped area in two nearest ref frames
void get_ref_skip_percentage(PictureControlSet *pcs_ptr, uint8_t *skip_area) {

    assert(skip_area != NULL);
    if (pcs_ptr->slice_type == I_SLICE) {
        *skip_area = 0;
        return;
    }

    uint8_t skip_perc = 0;
    EbReferenceObject *ref_obj_l0 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_0][0]->object_ptr;
    skip_perc = ref_obj_l0->skip_coded_area;
    if (pcs_ptr->slice_type == B_SLICE) {
        EbReferenceObject *ref_obj_l1 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_1][0]->object_ptr;
        skip_perc += ref_obj_l1->skip_coded_area;

        // if have two frames, divide the skip_perc by 2 to get the avg skip area
        skip_perc >>= 1;
    }

    *skip_area = skip_perc;
}
#else
#if OPT_CDEF
extern uint8_t ref_is_high_skip(PictureControlSet *pcs_ptr, uint8_t *skip_area);
#endif
#endif
#if CLN_RTIME_MEM_ALLOC
void* rtime_alloc_block_hash_block_is_same(size_t size) {
        return malloc(size);
    }
#endif
/* Mode Decision Configuration Kernel */

/*********************************************************************************
*
* @brief
*  The Mode Decision Configuration Process involves a number of initialization steps,
*  setting flags for a number of features, and determining the blocks to be considered
*  in subsequent MD stages.
*
* @par Description:
*  The Mode Decision Configuration Process involves a number of initialization steps,
*  setting flags for a number of features, and determining the blocks to be considered
*  in subsequent MD stages. Examples of flags that are set are the flags for filter intra,
*  eighth-pel, OBMC and warped motion and flags for updating the cumulative density functions
*  Examples of initializations include initializations for picture chroma QP offsets,
*  CDEF strength, self-guided restoration filter parameters, quantization parameters,
*  lambda arrays, mv and coefficient rate estimation arrays.
*
*  The set of blocks to be processed in subsequent MD stages is decided in this process as a
*  function of the picture depth mode (pic_depth_mode).
*
* @param[in] Configurations
*  Configuration flags that are to be set
*
* @param[out] Initializations
*  Initializations for various flags and variables
*
********************************************************************************/
void *mode_decision_configuration_kernel(void *input_ptr) {
    // Context & SCS & PCS
    EbThreadContext *                 thread_context_ptr = (EbThreadContext *)input_ptr;
    ModeDecisionConfigurationContext *context_ptr        = (ModeDecisionConfigurationContext *)
                                                        thread_context_ptr->priv;
    // Input
    EbObjectWrapper *rate_control_results_wrapper_ptr;

    // Output
    EbObjectWrapper *enc_dec_tasks_wrapper_ptr;

    for (;;) {
        // Get RateControl Results
        EB_GET_FULL_OBJECT(context_ptr->rate_control_input_fifo_ptr,
                           &rate_control_results_wrapper_ptr);

        RateControlResults *rate_control_results_ptr =
            (RateControlResults *)rate_control_results_wrapper_ptr->object_ptr;
        PictureControlSet *pcs_ptr = (PictureControlSet *)
                                         rate_control_results_ptr->pcs_wrapper_ptr->object_ptr;
        SequenceControlSet *scs_ptr = (SequenceControlSet *)pcs_ptr->scs_wrapper_ptr->object_ptr;

        // -------
        // Scale references if resolution of the reference is different than the input
        // -------
        if (pcs_ptr->parent_pcs_ptr->frame_superres_enabled == 1 &&
            pcs_ptr->slice_type != I_SLICE) {
            if (pcs_ptr->parent_pcs_ptr->is_used_as_reference_flag == EB_TRUE &&
                pcs_ptr->parent_pcs_ptr->reference_picture_wrapper_ptr != NULL) {
                // update mi_rows and mi_cols for the reference pic wrapper (used in mfmv for other pictures)
                EbReferenceObject *reference_object =
                    pcs_ptr->parent_pcs_ptr->reference_picture_wrapper_ptr->object_ptr;
                reference_object->mi_rows = pcs_ptr->parent_pcs_ptr->aligned_height >> MI_SIZE_LOG2;
                reference_object->mi_cols = pcs_ptr->parent_pcs_ptr->aligned_width >> MI_SIZE_LOG2;
            }

            scale_rec_references(
                pcs_ptr, pcs_ptr->parent_pcs_ptr->enhanced_picture_ptr, pcs_ptr->hbd_mode_decision);
        }



        if (pcs_ptr->slice_type != I_SLICE && scs_ptr->mfmv_enabled)
            av1_setup_motion_field(pcs_ptr->parent_pcs_ptr->av1_cm, pcs_ptr);

        FrameHeader *frm_hdr = &pcs_ptr->parent_pcs_ptr->frm_hdr;
#if SS_FIX_MOVE_SKIP_INTRA_PIC
#if CLN_REF_AREA
        // Get intra % in ref frame
        get_ref_intra_percentage(pcs_ptr, &pcs_ptr->ref_intra_percentage);

        // Get skip % in ref frame
        get_ref_skip_percentage(pcs_ptr, &pcs_ptr->ref_skip_percentage);
#else
        // Get intra % in ref frame
        get_ref_intra_percentage(pcs_ptr, &pcs_ptr->intra_percentage);
#endif
#endif
        // Mode Decision Configuration Kernel Signal(s) derivation
        if (use_output_stat(scs_ptr))
            first_pass_signal_derivation_mode_decision_config_kernel(pcs_ptr);
        else
            signal_derivation_mode_decision_config_kernel_oq(scs_ptr, pcs_ptr);

#if TUNE_NEW_M11_2 && !SS_FIX_MOVE_SKIP_INTRA_PIC
        // Get intra % in ref frame
        get_ref_intra_percentage(pcs_ptr, &pcs_ptr->intra_percentage);
#endif
#if !RFCTR_RC_P2
        pcs_ptr->parent_pcs_ptr->average_qp = 0;
#endif
#if  FTR_INTRA_DETECTOR
        pcs_ptr->intra_coded_area           = 0;
#endif
#if FTR_COEFF_DETECTOR
        pcs_ptr->skip_coded_area = 0;
#endif
        // Init block selection
        // Set reference sg ep
        set_reference_sg_ep(pcs_ptr);
        set_global_motion_field(pcs_ptr);

        svt_av1_qm_init(pcs_ptr->parent_pcs_ptr);
        MdRateEstimationContext *md_rate_estimation_array;

        // QP
        context_ptr->qp = pcs_ptr->picture_qp;

        // QP Index
        context_ptr->qp_index = (uint8_t)frm_hdr->quantization_params.base_q_idx;

        md_rate_estimation_array = pcs_ptr->md_rate_estimation_array;
#if !CLN_MDCONTEXT
        // Reset MD rate Estimation table to initial values by copying from md_rate_estimation_array
        if (context_ptr->is_md_rate_estimation_ptr_owner) {
            EB_FREE_ARRAY(context_ptr->md_rate_estimation_ptr);
            context_ptr->is_md_rate_estimation_ptr_owner = EB_FALSE;
        }
        context_ptr->md_rate_estimation_ptr = md_rate_estimation_array;
#endif
        if (pcs_ptr->parent_pcs_ptr->frm_hdr.primary_ref_frame != PRIMARY_REF_NONE)
            memcpy(&pcs_ptr->md_frame_context,
                   &pcs_ptr->ref_frame_context[pcs_ptr->parent_pcs_ptr->frm_hdr.primary_ref_frame],
                   sizeof(FRAME_CONTEXT));
        else {
            svt_av1_default_coef_probs(&pcs_ptr->md_frame_context,
                                       frm_hdr->quantization_params.base_q_idx);
            init_mode_probs(&pcs_ptr->md_frame_context);
        }
        // Initial Rate Estimation of the syntax elements
#if OPT8_MDC
        av1_estimate_syntax_rate(md_rate_estimation_array,
            pcs_ptr->slice_type == I_SLICE ? EB_TRUE : EB_FALSE,
            pcs_ptr->pic_filter_intra_level,
            pcs_ptr->parent_pcs_ptr->frm_hdr.allow_screen_content_tools,
            scs_ptr->seq_header.enable_restoration,
            pcs_ptr->parent_pcs_ptr->frm_hdr.allow_intrabc,
#if OPT9_RATE_ESTIMATION
            pcs_ptr->parent_pcs_ptr->partition_contexts,
#endif
            &pcs_ptr->md_frame_context);
#else
        av1_estimate_syntax_rate(md_rate_estimation_array,
                                 pcs_ptr->slice_type == I_SLICE ? EB_TRUE : EB_FALSE,
                                 &pcs_ptr->md_frame_context);
#endif
        // Initial Rate Estimation of the Motion vectors
        if (!use_output_stat(scs_ptr)){
        av1_estimate_mv_rate(pcs_ptr, md_rate_estimation_array, &pcs_ptr->md_frame_context);
        // Initial Rate Estimation of the quantized coefficients
        av1_estimate_coefficients_rate(md_rate_estimation_array, &pcs_ptr->md_frame_context);
        }
        if (frm_hdr->allow_intrabc) {
            int            i;
            int            speed          = 1;
            SpeedFeatures *sf             = &pcs_ptr->sf;
            sf->allow_exhaustive_searches = 1;

            const int mesh_speed = AOMMIN(speed, MAX_MESH_SPEED);
#if !RFCTR_RC_P1
            //if (cpi->twopass.fr_content_type == FC_GRAPHICS_ANIMATION)
            //    sf->exhaustive_searches_thresh = (1 << 24);
            //else
#endif
            sf->exhaustive_searches_thresh = (1 << 25);

            sf->max_exaustive_pct = good_quality_max_mesh_pct[mesh_speed];
            if (mesh_speed > 0)
                sf->exhaustive_searches_thresh = sf->exhaustive_searches_thresh << 1;

            for (i = 0; i < MAX_MESH_STEP; ++i) {
                sf->mesh_patterns[i].range    = good_quality_mesh_patterns[mesh_speed][i].range;
                sf->mesh_patterns[i].interval = good_quality_mesh_patterns[mesh_speed][i].interval;
            }

            if (pcs_ptr->slice_type == I_SLICE) {
                for (i = 0; i < MAX_MESH_STEP; ++i) {
                    sf->mesh_patterns[i].range    = intrabc_mesh_patterns[mesh_speed][i].range;
                    sf->mesh_patterns[i].interval = intrabc_mesh_patterns[mesh_speed][i].interval;
                }
                sf->max_exaustive_pct = intrabc_max_mesh_pct[mesh_speed];
            }

            {
                // add to hash table
                const int pic_width  = pcs_ptr->parent_pcs_ptr->aligned_width;
                const int pic_height = pcs_ptr->parent_pcs_ptr->aligned_height;

                uint32_t *block_hash_values[2][2];
                int8_t *  is_block_same[2][3];
                int       k, j;

                for (k = 0; k < 2; k++) {
                    for (j = 0; j < 2; j++)
#if CLN_RTIME_MEM_ALLOC
                        block_hash_values[k][j] = rtime_alloc_block_hash_block_is_same(sizeof(uint32_t) * pic_width * pic_height);
                    for (j = 0; j < 3; j++)
                        is_block_same[k][j] = rtime_alloc_block_hash_block_is_same(sizeof(int8_t) * pic_width * pic_height);
#else
                        block_hash_values[k][j] = malloc(sizeof(uint32_t) * pic_width * pic_height);
                    for (j = 0; j < 3; j++)
                        is_block_same[k][j] = malloc(sizeof(int8_t) * pic_width * pic_height);
#endif
                }
#if OPT_MEMORY_HASH_TABLE
                pcs_ptr->hash_table.p_lookup_table = NULL;
#if CLN_RTIME_MEM_ALLOC
                rtime_alloc_svt_av1_hash_table_create(&pcs_ptr->hash_table);
#else
                svt_av1_hash_table_create(&pcs_ptr->hash_table);
#endif
#endif
                Yv12BufferConfig cpi_source;
                link_eb_to_aom_buffer_desc_8bit(pcs_ptr->parent_pcs_ptr->enhanced_picture_ptr,
                                                &cpi_source);

                svt_av1_crc_calculator_init(&pcs_ptr->crc_calculator1, 24, 0x5D6DCB);
                svt_av1_crc_calculator_init(&pcs_ptr->crc_calculator2, 24, 0x864CFB);

                svt_av1_generate_block_2x2_hash_value(
                    &cpi_source, block_hash_values[0], is_block_same[0], pcs_ptr);
#if SS_OPT_INTRABC
                uint8_t src_idx = 0;
#if OPT_IBC_HASH_SEARCH
                const uint8_t max_sb_size = pcs_ptr->parent_pcs_ptr->intraBC_ctrls.max_block_size_hash;
#else
                const uint16_t max_sb_size = (uint16_t)scs_ptr->static_config.super_block_size;
#endif
                for (int size = 4; size <= max_sb_size; size <<= 1, src_idx = !src_idx) {
                    const uint8_t dst_idx = !src_idx;
                    svt_av1_generate_block_hash_value(&cpi_source,
                                                      size,
                                                      block_hash_values[src_idx],
                                                      block_hash_values[dst_idx],
                                                      is_block_same[src_idx],
                                                      is_block_same[dst_idx],
                                                      pcs_ptr);
#if CLN_RTIME_MEM_ALLOC
#if OPT_IBC_HASH_SEARCH
                    if (size != 4 || pcs_ptr->parent_pcs_ptr->intraBC_ctrls.hash_4x4_blocks)
#endif
                    rtime_alloc_svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                        block_hash_values[dst_idx],
                        is_block_same[dst_idx][2],
                        pic_width,
                        pic_height,
                        size);
#else
                    svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                                                                    block_hash_values[dst_idx],
                                                                    is_block_same[dst_idx][2],
                                                                    pic_width,
                                                                    pic_height,
                                                                    size);
#endif
                }
#else
                svt_av1_generate_block_hash_value(&cpi_source,
                                                  4,
                                                  block_hash_values[0],
                                                  block_hash_values[1],
                                                  is_block_same[0],
                                                  is_block_same[1],
                                                  pcs_ptr);
                svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                                                                block_hash_values[1],
                                                                is_block_same[1][2],
                                                                pic_width,
                                                                pic_height,
                                                                4);
                svt_av1_generate_block_hash_value(&cpi_source,
                                                  8,
                                                  block_hash_values[1],
                                                  block_hash_values[0],
                                                  is_block_same[1],
                                                  is_block_same[0],
                                                  pcs_ptr);
                svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                                                                block_hash_values[0],
                                                                is_block_same[0][2],
                                                                pic_width,
                                                                pic_height,
                                                                8);
                svt_av1_generate_block_hash_value(&cpi_source,
                                                  16,
                                                  block_hash_values[0],
                                                  block_hash_values[1],
                                                  is_block_same[0],
                                                  is_block_same[1],
                                                  pcs_ptr);
                svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                                                                block_hash_values[1],
                                                                is_block_same[1][2],
                                                                pic_width,
                                                                pic_height,
                                                                16);
                svt_av1_generate_block_hash_value(&cpi_source,
                                                  32,
                                                  block_hash_values[1],
                                                  block_hash_values[0],
                                                  is_block_same[1],
                                                  is_block_same[0],
                                                  pcs_ptr);
                svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                                                                block_hash_values[0],
                                                                is_block_same[0][2],
                                                                pic_width,
                                                                pic_height,
                                                                32);
                svt_av1_generate_block_hash_value(&cpi_source,
                                                  64,
                                                  block_hash_values[0],
                                                  block_hash_values[1],
                                                  is_block_same[0],
                                                  is_block_same[1],
                                                  pcs_ptr);
                svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                                                                block_hash_values[1],
                                                                is_block_same[1][2],
                                                                pic_width,
                                                                pic_height,
                                                                64);

                svt_av1_generate_block_hash_value(&cpi_source,
                                                  128,
                                                  block_hash_values[1],
                                                  block_hash_values[0],
                                                  is_block_same[1],
                                                  is_block_same[0],
                                                  pcs_ptr);
                svt_av1_add_to_hash_map_by_row_with_precal_data(&pcs_ptr->hash_table,
                                                                block_hash_values[0],
                                                                is_block_same[0][2],
                                                                pic_width,
                                                                pic_height,
                                                                128);
#endif
                for (k = 0; k < 2; k++) {
                    for (j = 0; j < 2; j++) free(block_hash_values[k][j]);
                    for (j = 0; j < 3; j++) free(is_block_same[k][j]);
                }
            }

            svt_av1_init3smotion_compensation(
                &pcs_ptr->ss_cfg, pcs_ptr->parent_pcs_ptr->enhanced_picture_ptr->stride_y);
        }
#if MS_CDEF_OPT3
        CdefControls *cdef_ctrls = &pcs_ptr->parent_pcs_ptr->cdef_ctrls;
#if OPT_CDEF
#if CLN_REF_AREA
        uint8_t skip_perc = pcs_ptr->ref_skip_percentage;
        if (skip_perc > 75 && cdef_ctrls->use_skip_detector)
#else
        uint8_t skip_area = 0;
        ref_is_high_skip(pcs_ptr, &skip_area);
        if (skip_area > 150 && cdef_ctrls->use_skip_detector)
#endif
            pcs_ptr->parent_pcs_ptr->cdef_level = 0;
        else {
#endif
        if (cdef_ctrls->use_reference_cdef_fs) {
            if (pcs_ptr->slice_type != I_SLICE) {
                uint8_t lowest_sg = TOTAL_STRENGTHS - 1;
                uint8_t highest_sg = 0;
                // Determine luma pred filter
                // Add filter from list0
                EbReferenceObject *ref_obj_l0 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_0][0]->object_ptr;
                for (uint8_t fs = 0; fs < ref_obj_l0->ref_cdef_strengths_num; fs++) {
                     if (ref_obj_l0->ref_cdef_strengths[0][fs] < lowest_sg)
                        lowest_sg = ref_obj_l0->ref_cdef_strengths[0][fs];
                     if (ref_obj_l0->ref_cdef_strengths[0][fs] > highest_sg)
                         highest_sg = ref_obj_l0->ref_cdef_strengths[0][fs];
                }
                if (pcs_ptr->slice_type == B_SLICE) {
                    // Add filter from list1
                    EbReferenceObject *ref_obj_l1 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_1][0]->object_ptr;
                    for (uint8_t fs = 0; fs < ref_obj_l1->ref_cdef_strengths_num; fs++) {
                        if (ref_obj_l1->ref_cdef_strengths[0][fs] < lowest_sg)
                            lowest_sg = ref_obj_l1->ref_cdef_strengths[0][fs];
                        if (ref_obj_l1->ref_cdef_strengths[0][fs] > highest_sg)
                            highest_sg = ref_obj_l1->ref_cdef_strengths[0][fs];
                    }
                }
                int8_t mid_filter =  MIN(63,MAX(0,(lowest_sg + highest_sg) / 2));
                cdef_ctrls->pred_y_f = mid_filter;
                cdef_ctrls->pred_uv_f = 0;
                cdef_ctrls->first_pass_fs_num = 0;
                cdef_ctrls->default_second_pass_fs_num = 0;
                // Set cdef to off if pred is.
                if ((cdef_ctrls->pred_y_f == 0) && (cdef_ctrls->pred_uv_f == 0))
                    pcs_ptr->parent_pcs_ptr->cdef_level = 0;
            }
        }
#if FTR_CDEF_SEARCH_BEST_REF
        else if (cdef_ctrls->search_best_ref_fs) {
            if (pcs_ptr->slice_type != I_SLICE) {
                cdef_ctrls->first_pass_fs_num = 1;
                cdef_ctrls->default_second_pass_fs_num = 0;

                // Add filter from list0, if not the same as the default
                EbReferenceObject *ref_obj_l0 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_0][0]->object_ptr;
                if (ref_obj_l0->ref_cdef_strengths[0][0] != cdef_ctrls->default_first_pass_fs[0]) {
                    cdef_ctrls->default_first_pass_fs[1] = ref_obj_l0->ref_cdef_strengths[0][0];
                    (cdef_ctrls->first_pass_fs_num)++;
                }

                if (pcs_ptr->slice_type == B_SLICE) {
                    EbReferenceObject *ref_obj_l1 = (EbReferenceObject *)pcs_ptr->ref_pic_ptr_array[REF_LIST_1][0]->object_ptr;
                    // Add filter from list1, if different from default filter and list0 filter
                    if (ref_obj_l1->ref_cdef_strengths[0][0] != cdef_ctrls->default_first_pass_fs[0] &&
                        ref_obj_l1->ref_cdef_strengths[0][0] != cdef_ctrls->default_first_pass_fs[cdef_ctrls->first_pass_fs_num - 1]) {

                        cdef_ctrls->default_first_pass_fs[cdef_ctrls->first_pass_fs_num] = ref_obj_l1->ref_cdef_strengths[0][0];
                        (cdef_ctrls->first_pass_fs_num)++;

                        // Chroma
                        if (ref_obj_l0->ref_cdef_strengths[1][0] == cdef_ctrls->default_first_pass_fs_uv[0] &&
                            ref_obj_l1->ref_cdef_strengths[1][0] == cdef_ctrls->default_first_pass_fs_uv[0]) {
                            cdef_ctrls->default_first_pass_fs_uv[0] = -1;
                            cdef_ctrls->default_first_pass_fs_uv[1] = -1;
                        }
                    }
                    // if list0/list1 filters are the same, skip CDEF search, and use the filter selected by the ref frames
                    else if (cdef_ctrls->first_pass_fs_num == 2 && ref_obj_l0->ref_cdef_strengths[0][0] == ref_obj_l1->ref_cdef_strengths[0][0]) {
                        cdef_ctrls->use_reference_cdef_fs = 1;

                        cdef_ctrls->pred_y_f = ref_obj_l0->ref_cdef_strengths[0][0];
                        cdef_ctrls->pred_uv_f = MIN(63, MAX(0, (ref_obj_l0->ref_cdef_strengths[1][0] + ref_obj_l1->ref_cdef_strengths[1][0]) / 2));
                        cdef_ctrls->first_pass_fs_num = 0;
                        cdef_ctrls->default_second_pass_fs_num = 0;
                    }
                }
                // Chroma
                else if (ref_obj_l0->ref_cdef_strengths[1][0] == cdef_ctrls->default_first_pass_fs_uv[0]) {
                    cdef_ctrls->default_first_pass_fs_uv[0] = -1;
                    cdef_ctrls->default_first_pass_fs_uv[1] = -1;
                }

                // Set cdef to off if pred luma is.
                if (cdef_ctrls->first_pass_fs_num == 1)
                    pcs_ptr->parent_pcs_ptr->cdef_level = 0;
            }

        }
#if OPT_CDEF
        }
#endif
#endif
#endif
        // Post the results to the MD processes

        uint16_t tg_count = pcs_ptr->parent_pcs_ptr->tile_group_cols *
            pcs_ptr->parent_pcs_ptr->tile_group_rows;
        for (uint16_t tile_group_idx = 0; tile_group_idx < tg_count; tile_group_idx++) {
            svt_get_empty_object(context_ptr->mode_decision_configuration_output_fifo_ptr,
                                 &enc_dec_tasks_wrapper_ptr);

            EncDecTasks *enc_dec_tasks_ptr = (EncDecTasks *)enc_dec_tasks_wrapper_ptr->object_ptr;
            enc_dec_tasks_ptr->pcs_wrapper_ptr  = rate_control_results_ptr->pcs_wrapper_ptr;
            enc_dec_tasks_ptr->input_type       = ENCDEC_TASKS_MDC_INPUT;
            enc_dec_tasks_ptr->tile_group_index = tile_group_idx;

            // Post the Full Results Object
            svt_post_full_object(enc_dec_tasks_wrapper_ptr);
        }

        // Release Rate Control Results
        svt_release_object(rate_control_results_wrapper_ptr);
    }

    return NULL;
}
