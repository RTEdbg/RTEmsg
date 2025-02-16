/*
 * Copyright (c) Branko Premzel.
 *
 * SPDX-License-Identifier: MIT
 */

/*******************************************************************************
 * @file    rtedbg.h
 * @author  B. Premzel
 * @brief   Structure definitions for data from the embedded system
 * @note    The structures defined here must be the same as in the embedded system
 *          library since the data is read from the files in the binary form.
 ******************************************************************************/

#ifndef  _RTEDBG_H
#define  _RTEDBG_H

#include "stdint.h"

#define MAX_MSG_LENGTH (256u * 4u)   // Limited by the rte_dbg structure header configuration

/***********************************************************************************
 * The configuration word defines embedded system RTEdbg configuration.
 * Bit    0: 0 - post-mortem logging is active (default)
 *           1 = single shot logging is active
 *        1: 1 = RTE_MSG_FILTERING_ENABLED
 *        2: 1 = RTE_FILTER_OFF_ENABLED
 *        3: 1 = RTE_SINGLE_SHOT_LOGGING_ENABLED
 *        4: 1 = RTE_USE_LONG_TIMESTAMP
 *  5 ..  7: reserved for future use (must be 0)
 *  8 .. 11: RTE_TIMESTAMP_SHIFT (0 = shift by 1)
 * 12 .. 14: RTE_FMT_ID_BITS   (0 = 9, 7 = 16)
 * 15      : reserved for future use (must be 0)
 * 16 .. 23: RTE_MAX_SUBPACKETS  (1 .. 256 - value 0 = 256)
 * 24 .. 30: RTE_HDR_SIZE (header size - number of 32b words)
 *       31: RTE_BUFF_SIZE_IS_POWER_OF_2 (1 = buffer size is power of 2, 0 - is not)
 ***********************************************************************************/
// Macros for parsing of rte_cfg from the rtedbg_header_t
#define RTE_SINGLE_SHOT_WAS_ACTIVE      ((g_msg.rte_header.rte_cfg  >>  0U) & 1U)
#define RTE_MSG_FILTERING_ENABLED       ((g_msg.rte_header.rte_cfg  >>  1U) & 1U)
#define RTE_FILTER_OFF_ENABLED          ((g_msg.rte_header.rte_cfg  >>  2U) & 1U)
#define RTE_SINGLE_SHOT_LOGGING_ENABLED ((g_msg.rte_header.rte_cfg  >>  3U) & 1U)
#define RTE_USE_LONG_TIMESTAMP          ((g_msg.rte_header.rte_cfg  >>  4U) & 1U)
#define RTE_CFG_RESERVED_BITS           ((g_msg.rte_header.rte_cfg  >>  5U) & 0x07U)
#define RTE_TIMESTAMP_SHIFT             (((g_msg.rte_header.rte_cfg >>  8U) & 0x0FU) + 1U)
#define RTE_FMT_ID_BITS                 ((g_msg.rte_header.rte_cfg  >> 12U) & 0x07U)
#define RTE_CFG_RESERVED2               ((g_msg.rte_header.rte_cfg  >> 15U) & 0x01U)
#define RTE_MAX_MSG_BLOCKS              (((g_msg.rte_header.rte_cfg >> 16U) & 0xFFU) ? \
                                         ((g_msg.rte_header.rte_cfg >> 16U) & 0xFFU) : 256U)
#define RTE_HEADER_SIZE                 (((g_msg.rte_header.rte_cfg >> 24U) & 0x7FU) * 4UL)
#define RTE_BUFF_SIZE_IS_POWER_OF_2     ((g_msg.rte_header.rte_cfg  >> 31U) & 1U)


//**** System message format IDs logged with the RTEdbg library functions ****
#define MSG1_SYS_LONG_TIMESTAMP     0
#define MSG1_SYS_TSTAMP_FREQUENCY   2
#define MSG1_SYS_STREAMING_MODE_LOGGING (g_msg.hdr_data.topmost_fmt_id)

/***** Special cases during various streaming logging modes ****
 * Data blocks with this index are added by the application running on the host computer during data 
 * transfer from embedded system to the host. The timestamp value in the FMT word has a different 
 * meaning - see the table on the following page. The timestamp value must not have a value with 
 * all ones since such message would produce a FMT word with a value of 0xFFFFFFFF which designates
 * non valid data (erased circular buffer value) => definition: bit 15 of timestamp value must be 0.
 */
#define SYS_HOST_DATE_TIME_INFO     0   // Message with date and time snapshot
#define SYS_DATA_OVERRUN_DETECTED   1   // Overrun during streaming mode logging with date/time info
#define SYS_MULTIPLE_LOGGING        2   // Snap-shot data with date/time info
// Values from 3 .. 15 reserved for future use


/**************************************************************************************
 * @brief Embedded system data logging structure header (without circular buffer).
 *************************************************************************************/
typedef struct
{
    volatile uint32_t last_index;
    /*!< Index to the circular data logging buffer.
     *   Points to the location where the next message will be written to. 
     *   If index points to the last four words of the circular buffer or one word after
     *   the end of buffer then the next word would be at the start of buffer.
     */
    volatile uint32_t filter;
    /*!< Enable/disable 32 message filters - each bit enables a group of messages. */
    /*   Bit 31 = filter #0, ... bit 0 = filter #31. */
    uint32_t rte_cfg;                   /*!< The RTEdbg configuration. */
    uint32_t timestamp_frequency;       /*!< Frequency of the timestamp counter [Hz]. */
    uint32_t filter_copy;
    /*!< Copy of the filter value to indicate the last non-zero value before the message
     * logging has been stopped. Either the host software or the firmware can restore
     * the value after the logging was temporarily stopped to, for example, transfer
     * a snapshot of the logging buffer to the host.
     */

    uint32_t buffer_size;               /*!< Size of the circular data logging buffer. */
        /* @note If streaming mode logging is used or the file contains data of several
         * snapshots then the value of 'buffer_size' is 0xFFFFFFFF. Length of raw data file
         * defines data quantity - as many data are processed as found in the file.
         */
} rtedbg_header_t;

#endif   // _RTEDBG_H

/*==== End of file ====*/
