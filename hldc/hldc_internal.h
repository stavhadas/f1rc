#include "hldc.h"

enum hldc_status hldc_push_byte(hldc_context_t *ctx, uint8_t byte);
void hldc_reset_context(hldc_context_t *ctx);
enum hldc_status hldc_handle_end_byte(hldc_context_t *ctx);