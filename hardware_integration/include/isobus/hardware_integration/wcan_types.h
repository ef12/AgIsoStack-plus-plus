/**
 * @file wcan_types.h
 * @brief Core WCAN types, status codes and portable helpers.
 *
 * Free of platform headers on purpose. Everything here is pure arithmetic or
 * validation over a frame, so it can be unit tested on any host and used by a
 * consumer that has not opened a bus yet.
 *
 * The transport API lives in wcan.h; the shared-memory layout that forms
 * the cross-process ABI is in wcan_layout.h.
 */

#ifndef WCAN_TYPES_H
#define WCAN_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WCAN_VERSION_MAJOR 2
#define WCAN_VERSION_MINOR 0
#define WCAN_VERSION_PATCH 0

/** Maximum payload bytes. Eight for classical CAN, 64 for CAN FD. */
#define WCAN_MAX_DATA 64u

/** Maximum bus name length, excluding the terminator. */
#define WCAN_MAX_BUS_NAME 63u

/** Blocks until a frame arrives. */
#define WCAN_INFINITE 0xffffffffu

/**
 * @brief Status codes returned by every WCAN call.
 *
 * The numbering is part of the ABI: the Python binding maps these values, so
 * existing codes must keep their meaning even when they fall out of use.
 */
enum {
    WCAN_OK = 0,
    WCAN_ERROR_INVALID_ARGUMENT = -1,
    WCAN_ERROR_NO_MEMORY = -2,
    WCAN_ERROR_UNAVAILABLE = -3,
    WCAN_ERROR_IO = -4,
    WCAN_ERROR_TIMEOUT = -5,
    WCAN_ERROR_PROTOCOL = -6,
    WCAN_ERROR_CLOSED = -7,
    WCAN_ERROR_ALREADY_OPEN = -8,
    WCAN_ERROR_INVALID_BUS = -9,
    WCAN_ERROR_INVALID_FRAME = -10,
    WCAN_ERROR_ACCESS_DENIED = -11,
    WCAN_ERROR_TOO_MANY_CLIENTS = -12
};

/** Frame format flags. */
enum {
    WCAN_FLAG_EXTENDED = 0x01u, /**< 29-bit identifier, as ISOBUS always uses */
    WCAN_FLAG_RTR = 0x02u,      /**< Remote frame; carries no payload */
    WCAN_FLAG_FD = 0x04u,       /**< CAN FD frame, up to 64 bytes */
    WCAN_FLAG_BRS = 0x08u,      /**< FD bit-rate switch */
    WCAN_FLAG_ESI = 0x10u       /**< FD error state indicator */
};

/**
 * @brief One CAN frame.
 *
 * @a dlc is the payload length in bytes, not the encoded DLC nibble.
 */
typedef struct wcan_frame {
    uint32_t can_id;
    uint8_t dlc;
    uint8_t flags;
    uint8_t data[WCAN_MAX_DATA];
} wcan_frame_t;

/**
 * @brief Validates a bus name.
 *
 * Names are 1 to WCAN_MAX_BUS_NAME ASCII letters, digits, dots, underscores
 * or hyphens, because they become Windows kernel object names.
 *
 * @param bus_name Name to check.
 * @return WCAN_OK, or WCAN_ERROR_INVALID_BUS.
 */
#ifdef __cplusplus
}
#endif

#endif /* WCAN_TYPES_H */