#include "controller.h"

controller_context_t controller_context;

int main()
{
  controller_init(&controller_context);
  controller_start(&controller_context);
}
