/**
 * @file wcan_airtime.h
 * @brief Bit-time model for classical CAN frames.
 *
 * Platform independent, so bus load can be reasoned about without opening a
 * bus and the model can be unit tested on any host.
 */

#ifndef WCAN_AIRTIME_H
#define WCAN_AIRTIME_H

#include "wcan_types.h"
#include "wcan_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the number of bits a frame occupies on the wire.
 *
 * Includes the stuff bits and the 13 unstuffed tail bits (CRC delimiter, ACK
 * slot, ACK delimiter, end of frame and interframe space), so the result is
 * the full time the frame owns the bus.
 *
 * With @a worst_case zero the frame is serialized, its CRC-15 computed and the
 * stuff bits counted exactly. With @a worst_case non-zero the maximum legal
 * stuff count is assumed instead, which is cheaper but overstates a typical
 * extended frame by around a fifth.
 *
 * CAN FD frames return a documented approximation, because FD uses different
 * stuff rules and a wider CRC.
 *
 * @param frame Frame to measure.
 * @param worst_case Non-zero to assume maximum stuffing.
 * @return Bit count, or zero if @a frame is NULL.
 */
WCAN_API uint32_t WCAN_CALL wcan_frame_bits(const wcan_frame_t *frame,
                                            int worst_case);

/**
 * @brief Returns how long a frame occupies the bus, in microseconds.
 *
 * @param frame Frame to measure.
 * @param bitrate Bus bitrate in bit/s.
 * @param worst_case Non-zero to assume maximum stuffing.
 * @return Airtime in microseconds, or zero if @a bitrate is zero.
 */
WCAN_API uint32_t WCAN_CALL wcan_frame_airtime_us(const wcan_frame_t *frame,
                                                  uint32_t bitrate,
                                                  int worst_case);

#ifdef __cplusplus
}
#endif

#endif /* WCAN_AIRTIME_H */
