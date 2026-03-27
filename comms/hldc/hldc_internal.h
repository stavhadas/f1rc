#include "hldc.h"

hldc_status_t hldc_push_byte(hldc_context_t *ctx, uint8_t byte);
void hldc_reset_context(hldc_context_t *ctx);
hldc_status_t hldc_handle_end_byte(hldc_context_t *ctx);
hldc_status_t hldc_insert_escape_char(uint8_t *buffer, size_t index, size_t buffer_length);