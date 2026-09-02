/**
 * @file wcan_validate.h
 * @brief Bus name and frame validation, and status code descriptions.
 *
 * Platform independent: a consumer can validate a frame before it ever opens
 * a bus, and these rules can be unit tested on any host.
 */

#ifndef WCAN_VALIDATE_H
#define WCAN_VALIDATE_H

#include "wcan_types.h"
#include "wcan_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validates a bus name.
 *
 * Names are 1 to WCAN_MAX_BUS_NAME ASCII letters, digits, dots, underscores
 * or hyphens, because they become Windows kernel object names.
 *
 * @param bus_name Name to check.
 * @return WCAN_OK, WCAN_ERROR_INVALID_ARGUMENT for NULL, or
 *         WCAN_ERROR_INVALID_BUS.
 */
WCAN_API int WCAN_CALL wcan_validate_bus_name(const char *bus_name);

/**
 * @brief Validates a frame against its declared format.
 *
 * Checks the identifier against the 11 or 29 bit limit implied by
 * WCAN_FLAG_EXTENDED, the payload length against the classical or FD limit,
 * and rejects flag combinations that cannot occur on a real bus, such as a
 * remote FD frame or a bit-rate switch on a classical frame.
 *
 * @param frame Frame to check.
 * @return WCAN_OK, WCAN_ERROR_INVALID_ARGUMENT for NULL, or
 *         WCAN_ERROR_INVALID_FRAME.
 */
WCAN_API int WCAN_CALL wcan_validate_frame(const wcan_frame_t *frame);

/**
 * @brief Returns a human readable description of a status code.
 *
 * @param status Any WCAN status code.
 * @return A static string; never NULL.
 */
WCAN_API const char *WCAN_CALL wcan_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* WCAN_VALIDATE_H */
