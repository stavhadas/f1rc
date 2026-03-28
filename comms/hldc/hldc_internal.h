#include <stdbool.h>
#include "hldc.h"
hldc_status_t hldc_handle_byte(hldc_context_t *ctx, uint8_t byte);
void hldc_restart_buffer(hldc_context_t *ctx, bool is_last_packet_successfull);
hldc_status_t hldc_handle_end_byte(hldc_context_t *ctx);
hldc_status_t hldc_insert_escape_char(uint8_t *buffer, size_t index, uint8_t byte, size_t buffer_length);