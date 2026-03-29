#ifndef SIMPLICITI_H
#define SIMPLICITI_H
#include "nwk.h"
#include "mfri.h"
#include "simplciti_common.h"

#define SIMPLICITI_MAX_FRAME_SIZE (sizeof(nwk_header_t) + sizeof(mfri_header_t) + NWK_MAX_PAYLOAD_SIZE)

typedef struct
{
    nwk_header_t nwk_header;
    mfri_header_t mfri_header;
    uint8_t payload[NWK_MAX_PAYLOAD_SIZE];
} simpliciti_frame_t;

simpliciti_status_t simpliciti_decode_frame(const uint8_t *data,
                                            size_t length,
                                            simpliciti_frame_t *frame);

simpliciti_status_t simpliciti_encode_frame(const simpliciti_frame_t *frame,
                                            uint8_t *buffer,
                                            size_t *length);

#endif // SIMPLICITI_H