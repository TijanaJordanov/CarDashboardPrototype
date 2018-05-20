#include "log_functions.h"
#include "rpi_gpio.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


/*
	Funkcija: export_pin  
	Eksportuje zadati pin 

	Argumenti:
		gpio_num - BCM broj pina

	Povratna vrednost: 
		0 - ok
		-1 - greska
 */
int export_pin(int gpio_num)
{
    int fd = open("/sys/class/gpio/export", O_WRONLY);

    if (gpio_num < 0 || gpio_num > 40) {
		log_data(2, " BCM number %d is not supported\n", gpio_num);
		return -1;
    }

    if (fd == -1) {
		log_data(2, " failed to open ~/sys/class/gpio/export~ file!\n");
		return -1;
    }

    log_data(0, "Exporting pin %d\n", gpio_num);

    char pin_data[4];
    sprintf(pin_data, "%d", gpio_num);

    int ret = write(fd, pin_data, strlen(pin_data));

    if (ret != -1)
		ret = 0;

    close(fd);

    return ret;
}

/*
	Funkcija: unexport_pin  
	Uklanja podatke za zadati pin 

	Argumenti:
		gpio_num - BCM broj pina

	Povratna vrednost: 
		0 - ok
		-1 - greska
 */
int unexport_pin(int gpio_num)
{
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);

    if (gpio_num < 0 || gpio_num > 40) {
		log_data(2, " BCM number %d is not supported\n", gpio_num);
		return -1;
    }

    if (fd == -1) {
		log_data(2, " failed to open ~/sys/class/gpio/unexport~ file!\n");
		return -1;
    }

    log_data(0, "Unexporting pin %d\n", gpio_num);

    char pin_data[4];

    sprintf(pin_data, "%d", gpio_num);

    int ret = write(fd, pin_data, strlen(pin_data));

    if (ret != -1)
		ret = 0;

    close(fd);

    return ret;
}

/*
	Funkcija: set_direction
	Postavljanje pravca za zadati pin 

	Argumenti:
		gpio_num - BCM broj pina
		direction - pravac (in/out)

	Povratna vrednost: 
		0 - ok
		-1 - greska
 */
int set_direction(int gpio_num, char *direction)
{
    char file_name[40];

    if (gpio_num < 0 || gpio_num > 40) {
		log_data(2, " BCM number %d is not supported\n", gpio_num);
		return -1;
    }

    sprintf(file_name, "/sys/class/gpio/gpio%d/direction", gpio_num);
    int fd = open(file_name, O_WRONLY);

    if (fd == -1) {
		log_data(2, "failed to open ~%s~ file!\n", file_name);
		return -1;
    }

    int ret = write(fd, direction, strlen(direction));

    if (ret != -1)
		ret = 0;

    close(fd);

    return ret;
}

/*
	Funkcija: get_value
	Cita vrednost pina

	Argumenti:
		gpio_num - BCM broj pina

	Povratna vrednost: 
		0 - ako je uspesno ocitana vrednost 0 ili je doslo do greske
		1 - ako je uspesno ocitana vrednost 1
 */
int get_value(int gpio_num)
{
    char file_name[40], value[2];

    if (gpio_num < 0 || gpio_num > 40) {
		log_data(2, " BCM number %d is not supported\n", gpio_num);
		return 0;
    }

    sprintf(file_name, "/sys/class/gpio/gpio%d/value", gpio_num);
    int fd = open(file_name, O_RDONLY);

    if (fd == -1) {
		log_data(2, "failed to open ~%s~ file!\n", file_name);
		return 0;
    }

    int r = read(fd, value, 2);	//0 ili 1

    if (r < 1) {
		log_data(2, "read in get_value(%d) returned %d\n", gpio_num, r);
		close(fd);
		return 0;
    }

    value[1] = '\0';

    close(fd);

    int ivalue = atoi(value);

    if (ivalue != 0 && ivalue != 1)
		ivalue = 0;

    return ivalue;
}
