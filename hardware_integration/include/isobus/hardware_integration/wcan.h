#ifndef WCAN_H
#define WCAN_H

/*
 * Prototype: brokerless WCAN transport built on a per-bus shared-memory
 * segment. Every process is a node; there is no wcand.exe.
 *
 * The API deliberately mirrors <wcan.h> so the two transports can be
 * benchmarked against each other without changing caller structure.
 */

#include <stdint.h>

#include "wcan_types.h"
#include "wcan_validate.h"
#include "wcan_airtime.h"
#include "wcan_export.h"

/* Receive frames this socket transmitted. Off by default, unlike the pipe
   broker, which always echoes. */
#define WCAN_OPEN_ECHO 0x01u

/* Bus-wide options, applied by whichever process creates the segment. */

/* Count the maximum legal stuff bits instead of serializing the real bit
   stream. Deterministic and cheap, but overstates bus load. */
#define WCAN_BUS_WORST_CASE_STUFFING 0x02u

/* Throttle transmitters so offered load cannot exceed the configured bitrate.
   Frames are still delivered as soon as they are committed. */
#define WCAN_BUS_PACE_ADMISSION 0x04u

/* Additionally hold each frame back from its receivers until its virtual
   transmission completes. Highest fidelity, highest CPU cost. */
#define WCAN_BUS_PACE_DELIVERY 0x08u

typedef struct {
    uint32_t bitrate;     /* 0 selects 250000 */
    uint32_t flags;
    uint32_t max_lead_us; /* admission pacing slack; 0 selects 2000 */
} wcan_params_t;

typedef struct {
    uint64_t frames;
    uint64_t bits;
    uint32_t bitrate;       /* effective bus bitrate */
    uint32_t segment_bytes; /* shared segment size for one bus */
    double bus_seconds;     /* virtual time occupied by frames */
    double elapsed_seconds; /* wall time since the first frame */
    double utilization;     /* bus_seconds / elapsed_seconds */
} wcan_bus_stats_t;

typedef struct wcan_socket {
    void *impl;
} wcan_socket_t;

#define WCAN_SOCKET_INITIALIZER { NULL }

#ifdef __cplusplus
extern "C" {
#endif

WCAN_API int WCAN_CALL wcan_open(wcan_socket_t *socket,
                                     const char *bus_name, uint32_t flags);
WCAN_API int WCAN_CALL wcan_open_ex(wcan_socket_t *socket,
                                        const char *bus_name,
                                        const wcan_params_t *params);
WCAN_API int WCAN_CALL wcan_send(wcan_socket_t *socket,
                                     const wcan_frame_t *frame);
WCAN_API int WCAN_CALL wcan_recv(wcan_socket_t *socket,
                                     wcan_frame_t *frame);
WCAN_API int WCAN_CALL wcan_recv_timeout(wcan_socket_t *socket,
                                             wcan_frame_t *frame,
                                             uint32_t timeout_ms);
WCAN_API int WCAN_CALL wcan_cancel(wcan_socket_t *socket);
WCAN_API int WCAN_CALL wcan_close(wcan_socket_t *socket);

WCAN_API int WCAN_CALL wcan_bus_stats(wcan_socket_t *socket,
                                          wcan_bus_stats_t *stats);

/* Introspection used by the tests and the benchmark. */
WCAN_API int WCAN_CALL wcan_node_index(wcan_socket_t *socket,
                                           uint32_t *out_index);
WCAN_API int WCAN_CALL wcan_overruns(wcan_socket_t *socket,
                                         uint32_t *out_overruns);
WCAN_API int WCAN_CALL wcan_node_count(wcan_socket_t *socket,
                                           uint32_t *out_count);

/*
 * Allocation helpers for runtimes that cannot express the socket struct, such
 * as ctypes. The handle is opaque and freed by wcan_handle_free().
 */
WCAN_API wcan_socket_t *WCAN_CALL wcan_handle_alloc(void);
WCAN_API void WCAN_CALL wcan_handle_free(wcan_socket_t *socket);

/* Library version, so a binding can verify the DLL it loaded. */
WCAN_API uint32_t WCAN_CALL wcan_abi_version(void);

/* Size of the shared segment, for diagnostics and layout verification. */
WCAN_API uint32_t WCAN_CALL wcan_segment_size(void);

#ifdef __cplusplus
}
#endif

#endif
