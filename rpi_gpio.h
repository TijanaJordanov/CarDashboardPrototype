#include <stdio.h>


int export_pin(int gpio_num);

int unexport_pin(int gpio_num);

int set_direction(int gpio_num, char *direction);

int get_value(int gpio_num);
