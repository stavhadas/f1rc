#include "car.h"

car_context_t car_context;

int main()
{
  car_init(&car_context);
  car_start(&car_context);
}