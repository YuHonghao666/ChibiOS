/*
    ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    hal_mmcsd.c
 * @brief   MMC/SD cards common code.
 *
 * @addtogroup MMCSD
 * @{
 */

#include "hal.h"

#if HAL_USE_MMC_SPI || HAL_USE_SDC || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Gets a bit field from a words array.
 * @note    The bit zero is the LSb of the first word.
 *
 * @param[in] data      pointer to the words array
 * @param[in] end       bit offset of the last bit of the field, inclusive
 * @param[in] start     bit offset of the first bit of the field, inclusive
 *
 * @return              The bits field value, left aligned.
 *
 * @notapi
 */
uint32_t _mmcsd_get_slice(const uint32_t *data,
                          uint32_t end,
                          uint32_t start) {
  unsigned startidx, endidx, startoff;
  uint32_t endmask;

  osalDbgCheck((end >= start) && ((end - start) < 32));

  startidx = start / 32;
  startoff = start % 32;
  endidx   = end / 32;
  endmask  = (1 << ((end % 32) + 1)) - 1;

  /* One or two pieces?*/
  if (startidx < endidx)
    return (data[startidx] >> startoff) |               /* Two pieces case. */
           ((data[endidx] & endmask) << (32 - startoff));
  return (data[startidx] & endmask) >> startoff;        /* One piece case.  */
}

/**
 * @brief   Extract card capacity from a CSD.
 * @details The capacity is returned as number of available blocks.
 *
 * @param[in] csd       the CSD record
 *
 * @return              The card capacity.
 * @retval 0            CSD format error
 *
 * @notapi
 */
uint32_t _mmcsd_get_capacity(const uint32_t *csd) {
  uint32_t a, b, c;

  osalDbgCheck(NULL != csd);

  switch (_mmcsd_get_slice(csd, MMCSD_CSD_10_CSD_STRUCTURE_SLICE)) {
  case 0:
    /* CSD version 1.0 */
    a = _mmcsd_get_slice(csd, MMCSD_CSD_10_C_SIZE_SLICE);
    b = _mmcsd_get_slice(csd, MMCSD_CSD_10_C_SIZE_MULT_SLICE);
    c = _mmcsd_get_slice(csd, MMCSD_CSD_10_READ_BL_LEN_SLICE);
    return (a + 1) << (b + 2) << (c - 9);       /* 2^9 == MMCSD_BLOCK_SIZE. */
  case 1:
    /* CSD version 2.0.*/
    return 1024 * (_mmcsd_get_slice(csd, MMCSD_CSD_20_C_SIZE_SLICE) + 1);
  default:
    /* Reserved value detected.*/
    return 0;
  }
}

/**
 * @brief   Extract MMC card capacity from EXT_CSD.
 * @details The capacity is returned as number of available blocks.
 *
 * @param[in] ext_csd   the extended CSD record
 *
 * @return              The card capacity.
 *
 * @notapi
 */
uint32_t _mmcsd_get_capacity_ext(const uint8_t *ext_csd) {

  osalDbgCheck(NULL != ext_csd);

  return (ext_csd[215] << 24) +
         (ext_csd[214] << 16) +
         (ext_csd[213] << 8)  +
          ext_csd[212];
}

/**
 * @brief   Unpacks SDC CID array in structure.
 *
 * @param[in] sdcp      pointer to the @p MMCSDBlockDevice object
 * @param[out] cidsdc   pointer to the @p unpacked_sdc_cid_t object
 *
 * @notapi
 */
void _mmcsd_unpack_sdc_cid(const MMCSDBlockDevice *sdcp,
                           unpacked_sdc_cid_t *cidsdc) {
  const uint32_t *cid;

  osalDbgCheck((NULL != sdcp) && (NULL != cidsdc));

  cid = sdcp->cid;
  cidsdc->crc    = _mmcsd_get_slice(cid, MMCSD_CID_SDC_CRC_SLICE);
  cidsdc->mdt_y  = _mmcsd_get_slice(cid, MMCSD_CID_SDC_MDT_Y_SLICE) + 2000;
  cidsdc->mdt_m  = _mmcsd_get_slice(cid, MMCSD_CID_SDC_MDT_M_SLICE);
  cidsdc->mid    = _mmcsd_get_slice(cid, MMCSD_CID_SDC_MID_SLICE);
  cidsdc->oid    = _mmcsd_get_slice(cid, MMCSD_CID_SDC_OID_SLICE);
  cidsdc->pnm[4] = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PNM0_SLICE);
  cidsdc->pnm[3] = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PNM1_SLICE);
  cidsdc->pnm[2] = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PNM2_SLICE);
  cidsdc->pnm[1] = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PNM3_SLICE);
  cidsdc->pnm[0] = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PNM4_SLICE);
  cidsdc->prv_n  = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PRV_N_SLICE);
  cidsdc->prv_m  = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PRV_M_SLICE);
  cidsdc->psn    = _mmcsd_get_slice(cid, MMCSD_CID_SDC_PSN_SLICE);
}

/**
 * @brief   Unpacks MMC CID array in structure.
 *
 * @param[in] sdcp      pointer to the @p MMCSDBlockDevice object
 * @param[out] cidmmc   pointer to the @p unpacked_mmc_cid_t object
 *
 * @notapi
 */
void _mmcsd_unpack_mmc_cid(const MMCSDBlockDevice *sdcp,
                           unpacked_mmc_cid_t *cidmmc) {
  const uint32_t *cid;

  osalDbgCheck((NULL != sdcp) && (NULL != cidmmc));

  cid = sdcp->cid;
  cidmmc->crc    = _mmcsd_get_slice(cid, MMCSD_CID_MMC_CRC_SLICE);
  cidmmc->mdt_y  = _mmcsd_get_slice(cid, MMCSD_CID_MMC_MDT_Y_SLICE) + 1997;
  cidmmc->mdt_m  = _mmcsd_get_slice(cid, MMCSD_CID_MMC_MDT_M_SLICE);
  cidmmc->mid    = _mmcsd_get_slice(cid, MMCSD_CID_MMC_MID_SLICE);
  cidmmc->oid    = _mmcsd_get_slice(cid, MMCSD_CID_MMC_OID_SLICE);
  cidmmc->pnm[5] = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PNM0_SLICE);
  cidmmc->pnm[4] = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PNM1_SLICE);
  cidmmc->pnm[3] = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PNM2_SLICE);
  cidmmc->pnm[2] = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PNM3_SLICE);
  cidmmc->pnm[1] = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PNM4_SLICE);
  cidmmc->pnm[0] = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PNM5_SLICE);
  cidmmc->prv_n  = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PRV_N_SLICE);
  cidmmc->prv_m  = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PRV_M_SLICE);
  cidmmc->psn    = _mmcsd_get_slice(cid, MMCSD_CID_MMC_PSN_SLICE);
}

/**
 * @brief   Unpacks MMC CSD array in structure.
 *
 * @param[in] sdcp      pointer to the @p MMCSDBlockDevice object
 * @param[out] csdmmc   pointer to the @p unpacked_mmc_csd_t object
 *
 * @notapi
 */
void _mmcsd_unpack_csd_mmc(const MMCSDBlockDevice *sdcp,
                           unpacked_mmc_csd_t *csdmmc) {
  const uint32_t *csd;

  osalDbgCheck((NULL != sdcp) && (NULL != csdmmc));

  csd = sdcp->csd;
  csdmmc->c_size             = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_C_SIZE_SLICE);
  csdmmc->c_size_mult        = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_C_SIZE_MULT_SLICE);
  csdmmc->ccc                = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_CCC_SLICE);
  csdmmc->copy               = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_COPY_SLICE);
  csdmmc->crc                = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_CRC_SLICE);
  csdmmc->csd_structure      = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_CSD_STRUCTURE_SLICE);
  csdmmc->dsr_imp            = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_DSR_IMP_SLICE);
  csdmmc->ecc                = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_ECC_SLICE);
  csdmmc->erase_grp_mult     = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_ERASE_GRP_MULT_SLICE);
  csdmmc->erase_grp_size     = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_ERASE_GRP_SIZE_SLICE);
  csdmmc->file_format        = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_FILE_FORMAT_SLICE);
  csdmmc->file_format_grp    = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_FILE_FORMAT_GRP_SLICE);
  csdmmc->nsac               = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_NSAC_SLICE);
  csdmmc->perm_write_protect = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_PERM_WRITE_PROTECT_SLICE);
  csdmmc->r2w_factor         = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_R2W_FACTOR_SLICE);
  csdmmc->read_bl_len        = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_READ_BL_LEN_SLICE);
  csdmmc->read_bl_partial    = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_READ_BL_PARTIAL_SLICE);
  csdmmc->read_blk_misalign  = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_READ_BLK_MISALIGN_SLICE);
  csdmmc->spec_vers          = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_SPEC_VERS_SLICE);
  csdmmc->taac               = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_TAAC_SLICE);
  csdmmc->tmp_write_protect  = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_TMP_WRITE_PROTECT_SLICE);
  csdmmc->tran_speed         = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_TRAN_SPEED_SLICE);
  csdmmc->vdd_r_curr_max     = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_VDD_R_CURR_MAX_SLICE);
  csdmmc->vdd_r_curr_min     = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_VDD_R_CURR_MIN_SLICE);
  csdmmc->vdd_w_curr_max     = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_VDD_W_CURR_MAX_SLICE);
  csdmmc->vdd_w_curr_min     = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_VDD_W_CURR_MIN_SLICE);
  csdmmc->wp_grp_enable      = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_WP_GRP_ENABLE_SLICE);
  csdmmc->wp_grp_size        = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_WP_GRP_SIZE_SLICE);
  csdmmc->write_bl_len       = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_WRITE_BL_LEN_SLICE);
  csdmmc->write_bl_partial   = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_WRITE_BL_PARTIAL_SLICE);
  csdmmc->write_blk_misalign = _mmcsd_get_slice(csd, MMCSD_CSD_MMC_WRITE_BLK_MISALIGN_SLICE);
}

/**
 * @brief   Unpacks SDC CSD v1.0 array in structure.
 *
 * @param[in] sdcp      pointer to the @p MMCSDBlockDevice object
 * @param[out] csd10    pointer to the @p unpacked_sdc_csd_10_t object
 *
 * @notapi
 */
void _mmcsd_unpack_csd_v10(const MMCSDBlockDevice *sdcp,
                           unpacked_sdc_csd_10_t *csd10) {
  const uint32_t *csd;

  osalDbgCheck(NULL != sdcp);

  csd = sdcp->csd;
  csd10->c_size              = _mmcsd_get_slice(csd, MMCSD_CSD_10_C_SIZE_SLICE);
  csd10->c_size_mult         = _mmcsd_get_slice(csd, MMCSD_CSD_10_C_SIZE_MULT_SLICE);
  csd10->ccc                 = _mmcsd_get_slice(csd, MMCSD_CSD_10_CCC_SLICE);
  csd10->copy                = _mmcsd_get_slice(csd, MMCSD_CSD_10_COPY_SLICE);
  csd10->crc                 = _mmcsd_get_slice(csd, MMCSD_CSD_10_CRC_SLICE);
  csd10->csd_structure       = _mmcsd_get_slice(csd, MMCSD_CSD_10_CSD_STRUCTURE_SLICE);
  csd10->dsr_imp             = _mmcsd_get_slice(csd, MMCSD_CSD_10_DSR_IMP_SLICE);
  csd10->erase_blk_en        = _mmcsd_get_slice(csd, MMCSD_CSD_10_ERASE_BLK_EN_SLICE);
  csd10->erase_sector_size   = _mmcsd_get_slice(csd, MMCSD_CSD_10_ERASE_SECTOR_SIZE_SLICE);
  csd10->file_format         = _mmcsd_get_slice(csd, MMCSD_CSD_10_FILE_FORMAT_SLICE);
  csd10->file_format_grp     = _mmcsd_get_slice(csd, MMCSD_CSD_10_FILE_FORMAT_GRP_SLICE);
  csd10->nsac                = _mmcsd_get_slice(csd, MMCSD_CSD_10_NSAC_SLICE);
  csd10->perm_write_protect  = _mmcsd_get_slice(csd, MMCSD_CSD_10_PERM_WRITE_PROTECT_SLICE);
  csd10->r2w_factor          = _mmcsd_get_slice(csd, MMCSD_CSD_10_R2W_FACTOR_SLICE);
  csd10->read_bl_len         = _mmcsd_get_slice(csd, MMCSD_CSD_10_READ_BL_LEN_SLICE);
  csd10->read_bl_partial     = _mmcsd_get_slice(csd, MMCSD_CSD_10_READ_BL_PARTIAL_SLICE);
  csd10->read_blk_misalign   = _mmcsd_get_slice(csd, MMCSD_CSD_10_READ_BLK_MISALIGN_SLICE);
  csd10->taac                = _mmcsd_get_slice(csd, MMCSD_CSD_10_TAAC_SLICE);
  csd10->tmp_write_protect   = _mmcsd_get_slice(csd, MMCSD_CSD_10_TMP_WRITE_PROTECT_SLICE);
  csd10->tran_speed          = _mmcsd_get_slice(csd, MMCSD_CSD_10_TRANS_SPEED_SLICE);
  csd10->wp_grp_enable       = _mmcsd_get_slice(csd, MMCSD_CSD_10_WP_GRP_ENABLE_SLICE);
  csd10->wp_grp_size         = _mmcsd_get_slice(csd, MMCSD_CSD_10_WP_GRP_SIZE_SLICE);
  csd10->write_bl_len        = _mmcsd_get_slice(csd, MMCSD_CSD_10_WRITE_BL_LEN_SLICE);
  csd10->write_bl_partial    = _mmcsd_get_slice(csd, MMCSD_CSD_10_WRITE_BL_PARTIAL_SLICE);
  csd10->write_blk_misalign  = _mmcsd_get_slice(csd, MMCSD_CSD_10_WRITE_BLK_MISALIGN_SLICE);
}

/**
 * @brief   Unpacks SDC CSD v2.0 array in structure.
 *
 * @param[in] sdcp      pointer to the @p MMCSDBlockDevice object
 * @param[out] csd20    pointer to the @p unpacked_sdc_csd_20_t object
 *
 * @notapi
 */
void _mmcsd_unpack_csd_v20(const MMCSDBlockDevice *sdcp,
                           unmacked_sdc_csd_20_t *csd20) {
  const uint32_t *csd;

  osalDbgCheck(NULL != sdcp);

  csd = sdcp->csd;
  csd20->c_size              = _mmcsd_get_slice(csd, MMCSD_CSD_20_C_SIZE_SLICE);
  csd20->crc                 = _mmcsd_get_slice(csd, MMCSD_CSD_20_CRC_SLICE);
  csd20->ccc                 = _mmcsd_get_slice(csd, MMCSD_CSD_20_CCC_SLICE);
  csd20->copy                = _mmcsd_get_slice(csd, MMCSD_CSD_20_COPY_SLICE);
  csd20->csd_structure       = _mmcsd_get_slice(csd, MMCSD_CSD_20_CSD_STRUCTURE_SLICE);
  csd20->dsr_imp             = _mmcsd_get_slice(csd, MMCSD_CSD_20_DSR_IMP_SLICE);
  csd20->erase_blk_en        = _mmcsd_get_slice(csd, MMCSD_CSD_20_ERASE_BLK_EN_SLICE);
  csd20->file_format         = _mmcsd_get_slice(csd, MMCSD_CSD_20_FILE_FORMAT_SLICE);
  csd20->file_format_grp     = _mmcsd_get_slice(csd, MMCSD_CSD_20_FILE_FORMAT_GRP_SLICE);
  csd20->nsac                = _mmcsd_get_slice(csd, MMCSD_CSD_20_NSAC_SLICE);
  csd20->perm_write_protect  = _mmcsd_get_slice(csd, MMCSD_CSD_20_PERM_WRITE_PROTECT_SLICE);
  csd20->r2w_factor          = _mmcsd_get_slice(csd, MMCSD_CSD_20_R2W_FACTOR_SLICE);
  csd20->read_bl_len         = _mmcsd_get_slice(csd, MMCSD_CSD_20_READ_BL_LEN_SLICE);
  csd20->read_bl_partial     = _mmcsd_get_slice(csd, MMCSD_CSD_20_READ_BL_PARTIAL_SLICE);
  csd20->read_blk_misalign   = _mmcsd_get_slice(csd, MMCSD_CSD_20_READ_BLK_MISALIGN_SLICE);
  csd20->erase_sector_size   = _mmcsd_get_slice(csd, MMCSD_CSD_20_ERASE_SECTOR_SIZE_SLICE);
  csd20->taac                = _mmcsd_get_slice(csd, MMCSD_CSD_20_TAAC_SLICE);
  csd20->tmp_write_protect   = _mmcsd_get_slice(csd, MMCSD_CSD_20_TMP_WRITE_PROTECT_SLICE);
  csd20->tran_speed          = _mmcsd_get_slice(csd, MMCSD_CSD_20_TRANS_SPEED_SLICE);
  csd20->wp_grp_enable       = _mmcsd_get_slice(csd, MMCSD_CSD_20_WP_GRP_ENABLE_SLICE);
  csd20->wp_grp_size         = _mmcsd_get_slice(csd, MMCSD_CSD_20_WP_GRP_SIZE_SLICE);
  csd20->write_bl_len        = _mmcsd_get_slice(csd, MMCSD_CSD_20_WRITE_BL_LEN_SLICE);
  csd20->write_bl_partial    = _mmcsd_get_slice(csd, MMCSD_CSD_20_WRITE_BL_PARTIAL_SLICE);
  csd20->write_blk_misalign  = _mmcsd_get_slice(csd, MMCSD_CSD_20_WRITE_BLK_MISALIGN_SLICE);
}

#endif /* HAL_USE_MMC_SPI || HAL_USE_SDC */

/** @} */
