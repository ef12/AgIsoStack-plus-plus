#ifndef WCAN_LAYOUT_H
#define WCAN_LAYOUT_H

/*
 * Shared-memory segment layout.
 *
 * This structure — not the library binary — is the real ABI between
 * processes. A 32-bit MinGW simulation, a 64-bit MSVC Python extension and the
 * Virtual Terminal all map the same bytes, so every field is fixed width,
 * every 64-bit field sits on an 8-byte boundary, and padding is explicit
 * rather than left to the compiler. The static assertions at the bottom fail
 * the build if any toolchain disagrees.
 */

#include <stddef.h>
#include <stdint.h>

#include "wcan_types.h"

#define WCAN_MAGIC 0x4d485357u /* 'WSHM' */
#define WCAN_LAYOUT_VERSION 2u
#define WCAN_MAX_NODES 64u
#define WCAN_RING_CAPACITY 128u
#define WCAN_MAX_PENDING 128u
#define WCAN_NAME_SIZE (WCAN_MAX_BUS_NAME + 1u)

#if defined(__cplusplus)
#define WCAN_STATIC_ASSERT(condition, name) static_assert(condition, #name)
#elif defined(_MSC_VER) && !defined(__STDC_VERSION__)
#define WCAN_STATIC_ASSERT(condition, name) \
    typedef char wcan_static_assert_##name[(condition) ? 1 : -1]
#else
#define WCAN_STATIC_ASSERT(condition, name) _Static_assert(condition, #name)
#endif

/** One frame in a receive ring. bus_time is the virtual instant at which the
    frame finished transmitting. */
typedef struct {
    uint32_t can_id;      /*  0 */
    uint8_t dlc;          /*  4 */
    uint8_t flags;        /*  5 */
    uint8_t reserved[2];  /*  6 */
    uint8_t data[WCAN_MAX_DATA]; /*  8 */
    uint64_t bus_time;    /* 72 */
    uint64_t sequence;    /* 80 */
} wcan_slot_t;        /* 88 */

/** A frame offered to the bus that has not yet won arbitration. */
typedef struct {
    wcan_slot_t slot; /*   0 */
    uint64_t arrival;     /*  88 */
    uint32_t sender;      /*  96 */
    uint32_t bits;        /* 100 */
} wcan_pending_t;     /* 104 */

/** Per-node state and its receive ring. */
typedef struct {
    uint32_t active;      /*  0 */
    uint32_t owner_pid;   /*  4 */
    uint32_t echo;        /*  8 */
    uint32_t head;        /* 12 */
    uint32_t count;       /* 16 */
    uint32_t overrun;     /* 20 */
    uint32_t reserved[2]; /* 24, aligns the ring to 8 */
    wcan_slot_t ring[WCAN_RING_CAPACITY]; /* 32 */
} wcan_node_t;

/** The whole bus. */
typedef struct {
    uint32_t magic;         /*  0 */
    uint32_t version;       /*  4 */
    uint32_t initialized;   /*  8 */
    uint32_t bitrate;       /* 12 */
    uint32_t flags;         /* 16 */
    uint32_t max_lead_us;   /* 20 */
    uint32_t pending_count; /* 24 */
    uint32_t reserved0;     /* 28 */
    uint64_t qpc_frequency; /* 32 */
    uint64_t sequence;      /* 40 */
    uint64_t bus_free_at;   /* 48 */
    uint64_t first_frame;   /* 56 */
    uint64_t total_frames;  /* 64 */
    uint64_t total_bits;    /* 72 */
    uint64_t queued_bits;   /* 80 */
    char bus_name[WCAN_NAME_SIZE];             /*  88 */
    wcan_pending_t pending[WCAN_MAX_PENDING]; /* 152 */
    wcan_node_t nodes[WCAN_MAX_NODES];
} wcan_bus_t;

WCAN_STATIC_ASSERT(sizeof(wcan_slot_t) == 88u, slot_size);
WCAN_STATIC_ASSERT(sizeof(wcan_pending_t) == 104u, pending_size);
WCAN_STATIC_ASSERT(sizeof(wcan_node_t) == 32u + (88u * WCAN_RING_CAPACITY),
                   node_size);
WCAN_STATIC_ASSERT(sizeof(wcan_bus_t) ==
                       152u + (104u * WCAN_MAX_PENDING) +
                           ((32u + (88u * WCAN_RING_CAPACITY)) *
                            WCAN_MAX_NODES),
                   bus_size);

WCAN_STATIC_ASSERT(offsetof(wcan_slot_t, data) == 8u, slot_data_offset);
WCAN_STATIC_ASSERT(offsetof(wcan_slot_t, bus_time) == 72u,
                   slot_bus_time_offset);
WCAN_STATIC_ASSERT(offsetof(wcan_pending_t, arrival) == 88u,
                   pending_arrival_offset);
WCAN_STATIC_ASSERT(offsetof(wcan_node_t, ring) == 32u, node_ring_offset);
WCAN_STATIC_ASSERT(offsetof(wcan_bus_t, qpc_frequency) == 32u,
                   bus_qpc_offset);
WCAN_STATIC_ASSERT(offsetof(wcan_bus_t, bus_name) == 88u, bus_name_offset);
WCAN_STATIC_ASSERT(offsetof(wcan_bus_t, pending) == 152u,
                   bus_pending_offset);

#endif /* WCAN_LAYOUT_H */
