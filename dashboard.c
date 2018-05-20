#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <GL/glut.h>
#include <GL/freeglut.h>

#include <png.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include "log_functions.h"
#include "rpi_gpio.h"

#include "configuration.h"

typedef struct Light {
    unsigned int gpio;		//gpio broj
    unsigned int status;	//da li treba da je upalim pri iscrtavanju
    unsigned int value;		//da li je u fajlu value 1 ili 0
} Light;

static void initialize(void);

static void reshape(int width, int height);
static void display(void);
static void on_timer(int value);
static void on_keyboard(unsigned char key, int x, int y);

static void init_png_light(GLuint * texture, int k, char *filename,
			   int format);
static void draw_light(GLuint * texture, int k, float start_x,
		       float start_y, float w, float h);

static void draw_side_gauge(float start_x, float start_y,
			    unsigned int proc, char color, int texture);
static void draw_center_of_scene(float start_x, float start_y);

static int read_i2c_data();
static void update_i2c_dependent_values();

static GLuint images[16];
static GLuint digits[10];

Light lights[10];		// 0- FL; 1- PB; 2- BL; 3- BF; 4- CE; 5- BC; 6- EO; 7- HL; 8- TL; 9- TR

char i2c_buffer[64] = { 0 };

int animation_ongoing = 0;	//regulise treperenje indikatora
int initialization_ongoing = 1;	//prvo iscrtavanje prikaza
unsigned int data_status = 0;	//status preuzetih podataka ok = 0, nok = 1

//i2c dependent values
unsigned int speed, collant_tmp, fuel, tacho_val, odom_val;

/*
	Funkcija: main    
	Glavna funkcija programa. Vrsi inicijalizaciju i pokrece obradu dogadjaja

	Argumenti:
		argc - broj argumenata
		argv - argumenti

	Povratna vrednost:
		0 - kraj funkcije
 */
int main(int argc, char **argv)
{
    GLfloat light_ambient[] = { 0.2, 0.2, 0.2, 1 };
    GLfloat light_diffuse[] = { 0.8, 0.8, 0.8, 1 };
    GLfloat light_specular[] = { 0.9, 0.9, 0.9, 1 };
    GLfloat light_position[] = { 0, 0.5, 0.5, 0 };

    //Inicijalizujem GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

    //kreiram prozor
    glutInitWindowSize(1024, 600);
    glutInitWindowPosition(0, 0);
    glutCreateWindow(argv[0]);

    //OpenGL inicijalizacija
    initialize();

    //registracija callback funkcija
    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(on_keyboard);

    //osvetljenje
    glShadeModel(GL_SMOOTH);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    //pocetak obrade dogadjaja
    glutMainLoop();

    return 0;
}


/*
	Funkcija: initialize  
	Vrsi OpenGl inicijalizaciju i inicijalizaciju podataka vezanih za pinove opste namene i arduino

	Argumenti: nema

	Povratna vrednost: nema
 */
static void initialize(void)
{
    int i, ret;

    //brisanje suvisnih logova preko niti
    pthread_t thread_id;

    ret = pthread_create(&thread_id, NULL, clear_old_logs,
		       "/home/pi/kontrolnaTabla/log/");

    if (ret == 0)
	//      pthread_join(thread_id, NULL);
		pthread_detach(thread_id);

    create_log_file("/home/pi/kontrolnaTabla/log/");

    //postavljam boju pozadine
    glClearColor(0, 0, 0, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glGenTextures(16, images);

    //ucitavanje svih tekstura za lampice
    init_png_light(images, 0, IMGFL_ON, 1);
    init_png_light(images, 1, IMGPB_ON, 1);
    init_png_light(images, 2, IMGBL_ON, 1);
    init_png_light(images, 3, IMGBF_ON, 1);
    init_png_light(images, 4, IMGCE_ON, 1);
    init_png_light(images, 5, IMGBC_ON, 1);
    init_png_light(images, 6, IMGEO_ON, 1);

    init_png_light(images, 7, IMGHL_ON, 1);
    init_png_light(images, 8, IMGTL_ON, 0);
    init_png_light(images, 9, IMGTR_ON, 0);

    init_png_light(images, 10, IMGCAR, 0);
    init_png_light(images, 11, IMGKMH, 0);

    init_png_light(images, 12, IMGCT, 1);
    init_png_light(images, 13, IMGFG, 1);

    init_png_light(images, 14, IMGWRN, 0);

    //ucitavanje tekstura za cifre
    glGenTextures(10, digits);
    init_png_light(digits, 0, IMGNUM_0, 0);
    init_png_light(digits, 1, IMGNUM_1, 0);
    init_png_light(digits, 2, IMGNUM_2, 0);
    init_png_light(digits, 3, IMGNUM_3, 0);
    init_png_light(digits, 4, IMGNUM_4, 0);
    init_png_light(digits, 5, IMGNUM_5, 0);
    init_png_light(digits, 6, IMGNUM_6, 0);
    init_png_light(digits, 7, IMGNUM_7, 0);
    init_png_light(digits, 8, IMGNUM_8, 0);
    init_png_light(digits, 9, IMGNUM_9, 0);


    speed = 0;

    //unosim gpio imena za indikatore
    lights[0].gpio = 16;
    lights[1].gpio = 19;
    lights[2].gpio = 13;
    lights[3].gpio = 12;
    lights[4].gpio = 25;
    lights[5].gpio = 6;
    lights[6].gpio = 5;
    lights[7].gpio = 21;
    lights[8].gpio = 26;
    lights[9].gpio = 20;

    //eksportujem pin-ove i Inicijalizujem svetla
    char *direction = "in";
    for (i = 0; i < 10; i++) {
		export_pin(lights[i].gpio);	//RPi: 
		set_direction(lights[i].gpio, direction);	//RPi: 

		lights[i].status = 1;
		lights[i].value = 1;
    }

    if (read_i2c_data() > 0) {
		update_i2c_dependent_values();
    } else {
		speed = 0;
		collant_tmp = 0;
		fuel = 0;
		tacho_val = 0;
		odom_val = 0;
    }

    log_data(0, "Dashboard init finished\n");
}


/*
	Funkcija: read_i2c_data  
	Cita podatke sa arduina

	Argumenti: nema

	Povratna vrednost: 
		1 - uspesno citanje podataka
		-1 - greska
 */
static int read_i2c_data()
{
    int i2c_file;
    i2c_file = open("/dev/i2c-1", O_RDWR);

    if (i2c_file < 0) {
		log_data(2, "failed to open i2c comunication files\n");
		data_status = 1;
		return -1;
    }


    int addr = 0x40;

    if (ioctl(i2c_file, I2C_SLAVE, addr) < 0) {
		log_data(2, " failed to access slave device\n");
		data_status = 1;
		return -1;
    }

    unsigned int length = 63;
    int k = read(i2c_file, i2c_buffer, length);

    if (k == -1) {
		log_data(2, " failed trying to read data from arduino\n");
		close(i2c_file);
		data_status = 1;
		return -1;
    }
    //log_data(0, "Read data: %s (%d)\n", i2c_buffer, k);

    close(i2c_file);

    return 1;
}


/*
	Funkcija: update_i2c_dependent_values
	Azurira varijable cije se vrednosti dobijaju sa arduina

	Argumenti: nema

	Povratna vrednost: nema
 */
static void update_i2c_dependent_values()
{
    char *pos = NULL;
    int length = 0;
    char tmp[8];

    if ((pos = strstr(i2c_buffer, "SPD")) != NULL)	//brzina
    {
		char *del = strchr(pos, '|');

		if (del != NULL) {
			length = del - pos - 3;

			if (length < 8) {
				strncpy(tmp, pos + 3, length);
				tmp[length] = '\0';
			}

			int pom = 0;
			pom = atoi(tmp);

			if (pom >= 0 && pom < 250)
				speed = pom;
		}
		//log_data(0, "detected speed %d\n", speed);
		pos = NULL;
    }


    if ((pos = strstr(i2c_buffer, "TMP")) != NULL)	//temperatura
    {
		char *del = strchr(pos, '|');

		if (del != NULL) {
			length = del - pos - 3;

			if (length < 8) {
				strncpy(tmp, pos + 3, length);
				tmp[length] = '\0';
			}

			int pom = 0;
			pom = atoi(tmp);

			if (pom >= 0 && pom < 101)
				collant_tmp = pom;
		}

		pos = NULL;
		//log_data(0, "collant_tmp %d\n", collant_tmp);
    }


    if ((pos = strstr(i2c_buffer, "FUE")) != NULL)	//nivo goriva
    {
		char *del = strchr(pos, '|');

		if (del != NULL) {
			length = del - pos - 3;

			if (length < 8) {
				strncpy(tmp, pos + 3, length);
				tmp[length] = '\0';
			}

			int pom = 0;
			pom = atoi(tmp);

			if (pom >= 0 && pom < 101)
				fuel = pom;
		}

		pos = NULL;
		//log_data(0, "fuel %d\n", fuel);
    }

    if ((pos = strstr(i2c_buffer, "TAC")) != NULL)	//tahometar
    {
		char *del = strchr(pos, '|');

		if (del != NULL) {
			length = del - pos - 3;

			if (length < 8) {
				strncpy(tmp, pos + 3, length);
				tmp[length] = '\0';
			}

			int pom = 0;
			pom = atoi(tmp);

			if (pom >= 0 && pom < 8)
				tacho_val = pom;
		}

		pos = NULL;
		//log_data( 0, "tacho_val %d\n", tacho_val);
    }

    if ((pos = strstr(i2c_buffer, "ODM")) != NULL)	//odometar
    {
		char *del = strchr(pos, '|');

		if (del != NULL) {
			length = del - pos - 3;

			if (length < 8) {
				strncpy(tmp, pos + 3, length);
				tmp[length] = '\0';
			}

			int pom = 0;
			pom = atoi(tmp);

			if (pom >= 0 && pom < 1000000)
				odom_val = pom;
		}

		pos = NULL;
		//log_data( 0, "odom_val %d\n", odom_val);
    }

}


/*
	Funkcija: on_keyboard
	Simulira vrednosti pracenih velicina detekcijom dogadjaja sa tastature

	Argumenti: 
		key - karakter pritisnut na tastaturi
		x, y - koordinate pozicije na ekranu na kojoj je kliknuto misem

	Povratna vrednost: nema
 */
static void on_keyboard(unsigned char key, int x, int y)
{
    int i;

    switch (key) {
    case 27:			//kraj, dugme esc
		for (i = 0; i < 10; i++)
			unexport_pin(lights[i].gpio);
		close_log_file();
		exit(0);
	break;
    case '1':
		lights[1].value = (lights[1].value + 1) % 2;
		log_data(0, "activated parking brake indicator\n");
	break;
    case '2':
		lights[2].value = (lights[2].value + 1) % 2;
		log_data(0, "activated brake lining indicator\n");
	break;
    case '3':
		lights[3].value = (lights[3].value + 1) % 2;
		log_data(0, "activated brake fluid indicator\n");
	break;
    case '4':
		lights[4].value = (lights[4].value + 1) % 2;
		log_data(0, "activated check engine indicator\n");
	break;
    case '5':
		lights[5].value = (lights[5].value + 1) % 2;
		log_data(0, "activated battery charge indicator\n");
	break;
    case '6':
		lights[6].value = (lights[6].value + 1) % 2;
		log_data(0, "activated engine oil indicator\n");
	break;
    case '7':
		lights[7].value = (lights[7].value + 1) % 2;
		log_data(0, "activated lights indicator\n");
	break;
    case '8':
		lights[0].value = (lights[0].value + 1) % 2;
		log_data(0, "activated fog ligth indicator\n");
	break;
    case '9':
		lights[8].value = (lights[8].value + 1) % 2;
		log_data(0, "activated turn left indicator\n");
	break;
    case '0':
		lights[9].value = (lights[9].value + 1) % 2;
		log_data(0, "activated turn right indicator\n");
	break;
    case '-':
		if (speed > -1)		//testiranje reagovanja na greske
			speed--;
		log_data(0, "activated speed %d\n", speed);
	break;
    case '=':
		if (speed < 250)
			speed++;
		log_data(0, "activated speed %d\n", speed);
	break;
    case 'a':
		if (fuel > -1)
			fuel--;
		log_data(0, "activated fuel %d\n", fuel);
	break;
    case 'q':
		if (fuel < 101)
			fuel++;
		log_data(0, "activated fuel %d\n", fuel);
	break;
    case 's':
		if (collant_tmp > -1)
			collant_tmp--;
		log_data(0, "activated collant_tmp %d\n", collant_tmp);
	break;
    case 'w':
		if (collant_tmp < 101)
			collant_tmp++;
		log_data(0, "activated collant_tmp %d\n", collant_tmp);
	break;
    case 'd':
		if (tacho_val > -1)
			tacho_val--;
		log_data(0, "activated tacho_val %d\n", tacho_val);
	break;
    case 'e':
		if (tacho_val < 8)
			tacho_val++;
		log_data(0, "activated tacho_val %d\n", tacho_val);
	break;
    case 'f':
		if (odom_val > -1)
			odom_val--;
		log_data(0, "activated odom_val %d\n", odom_val);
	break;
    case 'r':
		if (odom_val < 1000000)
			odom_val++;
		log_data(0, "activated odom_val %d\n", odom_val);
	break;
    default:
	break;
    }

    glutPostRedisplay();
}


/*
	Funkcija: on_timer
	Azuriranje vrednosti i iniciranje ponovnog iscrtavanja scene

	Argumenti: 
		value - id tajmera

	Povratna vrednost: nema
 */
static void on_timer(int value)
{
    int i;

    if (value != 0)
		return;

    animation_ongoing = (animation_ongoing + 1) % 2;	//kontrolise treperenje indikatora 

    //provera signala za indikatore
    for (i = 0; i < 10; i++) {
		lights[i].value = get_value(lights[i].gpio);	//RPi: 

		if (i == 0 || i == 7)
			if (lights[i].value > 0)
				lights[i].status = 1;
			else
				lights[i].status = 0;
		else 
			if (lights[i].value > 0)
				lights[i].status = animation_ongoing;	//1 - upali, 0 - ugasi
			else
				lights[i].status = 0;
    }

    //provera i2c podataka
    if (read_i2c_data() > 0) {
		update_i2c_dependent_values();
    }


    glutPostRedisplay();	//forsira ponovno iscrtavanje prozora

    glutTimerFunc(TIMER_INTERVAL, on_timer, TIMER_ID);
}


/*
	Funkcija: reshape
	Podesavanje prikaza

	Argumenti: 
		width - sirina prozora
		height - visina prozora

	Povratna vrednost: nema
 */
static void reshape(int width, int height)
{
    glViewport(0, 0, width, height);	//podesava pravougaonik prikazivanja na ekranu

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(20, (float) width / height, 10, 100);

}


/*
	Funkcija: draw_speed
	Crtanje elemenata za prikaz brzine

	Argumenti: 
		x_center - x koordinata donjeg levog ugla
		y_center - y koordinata donjeg levog ugla
		speed - brzina 

	Povratna vrednost:
		broj cifara u vrednosti za brzinu
 */
int draw_speed(float x_center, float y_center, int speed)
{
    int digits_num = 0;
    int tmp = speed;
    int speed_digits[4];

    if (speed < 0)
		return digits_num;

    if (tmp == 0) {
		speed_digits[0] = 0;
		digits_num = 1;
    } else
		while (tmp != 0 && digits_num < 4) {
			speed_digits[digits_num] = tmp % 10;	//popunjavam obrnutim redosledom
			digits_num++;
			tmp = tmp / 10;
		}

    float x_kmh = 0.3;
    float start_x = 0.3, start_y = y_center - 0.45;
    for (int i = 0; i < digits_num; i++) {

		if (digits_num > 1)
			start_x = digits_num % 2 == 0 ? x_center - 0.6 * i : 0.3 - i * 0.6;
		else
			start_x = x_center - 0.3;

		if (i == 0)
			x_kmh = start_x + 0.7;

		//log_data( 0, "texture num %d\n", speed_digits[i]);

		draw_light(digits, speed_digits[i], start_x, start_y, 0.6, 0.9);
    }

    draw_light(images, 11, x_kmh, start_y, 0.6, 0.5);

    return digits_num;
}


/*
	Funkcija: display
	Iscrtavanje elemenata na ekranu

	Argumenti:  nema

	Povratna vrednost: nema
 */
static void display(void)
{
    int i;
    GLfloat clWhite[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat clTile[] = { 0.29, 0.71, 0.74, 1.0 };
    GLfloat high_shininess[] = { 100 };

    //brise prethodni sadrzaj prozora
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //pozicioniranje kamere
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0.2, 10.5, 0, 0, 0, 0, 1, 0);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glColorMaterial(GL_FRONT, GL_SPECULAR);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMaterialfv(GL_FRONT, GL_SPECULAR, clWhite);
    glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);

    draw_center_of_scene(0, 0);

    draw_side_gauge(-2.25, -1.4, fuel, 'y', 13);
    draw_side_gauge(2.2, -1.4, collant_tmp, 'r', 12);

    //svetla upozorenja
    for (i = 0; i < 6; i++)
	if (lights[i + 1].status == 1)
	    draw_light(images, i + 1, -2.4 + (i % 3) * 0.4,  i > 2 ? 0.4 : 0.8, 0.4, 0.4);

    //svetla duga i za maglu
    if (lights[0].status == 1)
		draw_light(images, 0, 1.7, 0.8, 0.4, 0.4);
    if (lights[7].status == 1)
		draw_light(images, 7, 2.2, 0.8, 0.4, 0.4);

    //svetlo upozorenja kada  podaci sa arduina nisu uspesno ocitani
    if (data_status == 1) {
		draw_light(images, 14, 2.3, 0.45, 0.2, 0.25);
		data_status = 0;
    }

    glMaterialfv(GL_FRONT, GL_AMBIENT, clTile);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, clTile);
    glRasterPos2f(-0.2, -1.4);
    glLineWidth(7);

    char d[10];

    sprintf(d, "%.6d km", odom_val);
    glutBitmapString(GLUT_BITMAP_TIMES_ROMAN_24, (const unsigned char *) d);

    if (speed >= 0)
		draw_speed(0, 0.7, speed);

    draw_light(images, 10, -0.6, -1.2, 1.2, 0.8);

    //pokazivaci pravca
    if (lights[8].status == 1)
		draw_light(images, 8, -1.8, -0.6, 0.4, 0.8);
    if (lights[9].status == 1)
		draw_light(images, 9, 1.4, -0.6, 0.4, 0.8);

    //salje sliku na ekran
    glutSwapBuffers();

    //poziva on_timer funkciju prilikom prvog iscrtavanja
    if (initialization_ongoing == 1) {
		glutTimerFunc(2000, on_timer, TIMER_ID);
		initialization_ongoing = 0;
		log_data(0, "Started on_timer function\n");
    }

}


/*
	Funkcija: init_png_light
	Citanje piksela png slike

	Argumenti:  
		texture - niz tekstura 
		k - indeks teksture iz niza
		filename - putanja do slike
		type - format komponente za boju

	Povratna vrednost: nema
 */
static void init_png_light(GLuint * texture, int k, char *filename,
			   int format)
{
    //trebaju mi struct i info 
    png_structp png_ptr;
    png_infop info_ptr;

    //provera da li je png fajl
    unsigned char header[8];
    FILE *fp = fopen(filename, "rb");

    if (!fp) {
		log_data(2, "cannot open file %s\n", filename);
		return;
    }

    fread(header, 1, 8, fp);	//citam heder fajla

    if (png_sig_cmp(header, 0, 8))	//vraca 0 ako je heder png fajla
    {
		fclose(fp);
		log_data(2, "%s file is not png\n", filename);
		return;
    }
    //inicijalizacija
    png_ptr =
	png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL) {
		fclose(fp);
		log_data(2, "unable to create read structure for file %s\n", filename);
		return;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(fp);
		log_data(2, "unable to create info structure for file %s \n", filename);
		return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		log_data(2, "cannot save current execution context%s\n", filename);
		return;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);	//da obavesti da je 8 vec procitano

    png_read_info(png_ptr, info_ptr);
    png_uint_32 width, height;

    int bit_depth, color_type, interlace_type;

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
    png_read_update_info(png_ptr, info_ptr);

    unsigned char *pixels = NULL;
    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);

    //ovo sluzi ako slika nije dimenzija 2^x
    row_bytes += 3 - ((row_bytes - 1) % 4);

    pixels =	(unsigned char *) malloc(row_bytes * height *
				 sizeof(unsigned char) + 15);

    if (pixels == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		log_data(2, "malloc error in init_png_light()\n");
		return;
    }

    png_bytep *row_pointers =
	(png_bytep *) malloc(sizeof(png_bytep) * height);

    if (row_pointers == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(pixels);
		fclose(fp);
		log_data(2, "malloc error in init_png_light()\n");
		return;
    }

    for (int i = 0; i < height; i++) {
		row_pointers[height - 1 - i] = pixels + i * row_bytes;
    }

    png_read_image(png_ptr, row_pointers);

    glBindTexture(GL_TEXTURE_2D, texture[k]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    if (format == 1)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,  GL_UNSIGNED_BYTE, pixels);
    else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);


    glBindTexture(GL_TEXTURE_2D, 0);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    free(pixels);
    free(row_pointers);
    fclose(fp);
}


/*
	Funkcija: draw_light
	Crta poligon sa teksturom za indikator

	Argumenti:  
		texture - niz tekstura 
		k - indeks teksture iz niza
		start_x, start_y - pozicija donjeg levog ugla
		w - sirina
		h - visina

	Povratna vrednost: nema
 */
static void draw_light(GLuint * texture, int k, float start_x,
		       float start_y, float w, float h)
{

    glBindTexture(GL_TEXTURE_2D, texture[k]);
    glBegin(GL_POLYGON);

		glTexCoord2f(0, 0);
		glVertex3f(start_x, start_y, 0);
		glTexCoord2f(1, 0);
		glVertex3f(start_x + w, start_y, 0);
		glTexCoord2f(1, 1);
		glVertex3f(start_x + w, start_y + h, 0);
		glTexCoord2f(0, 1);
		glVertex3f(start_x, start_y + h, 0);

    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

}


/*
	Funkcija: draw_side_gauge
	Crta skale sa podeocima

	Argumenti:  
		start_x, start_y - pozicija donjeg levog ugla
		proc - procentualna vrednost koju treba prikazati na skali
		color - boja 
		texture - tekstura za simbol

	Povratna vrednost: nema
 */
static void draw_side_gauge(float start_x, float start_y,
			    unsigned int proc, char color, int texture)
{
    glTranslatef(start_x, start_y, 0);

    GLfloat clBlue[] = { 0.18, 0.28, 0.45, 1.0 };
    GLfloat clWhite[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat clLine[] = { 1.0, 0.0, 0.0, 1.0 };

    if (color == 'y') {
		clLine[1] = 0.94;
		clLine[2] = 0.05;
    }

    glMaterialfv(GL_FRONT, GL_SPECULAR, clWhite);

    glMaterialfv(GL_FRONT, GL_AMBIENT, clBlue);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, clBlue);

    //nacrtam liniju
    int i;
    for (i = 0; i < 5; i++) {
		if (i % 2 != 0)
			glLineWidth(5);
		else
			glLineWidth(7);

	glBegin(GL_LINES);
		glNormal3f(0, 0, 1);
		glVertex2f(-0.04, (i + 1) * 0.3);
		glVertex2f(0.04, (i + 1) * 0.3);
	glEnd();

    }

    glMaterialfv(GL_FRONT, GL_AMBIENT, clLine);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, clLine);

    //kazaljka
    glLineWidth(5);
    glBegin(GL_LINES);
		glNormal3f(0, 0, 1);
		glVertex3f(0, 0, -0.009);
		glVertex3f(0, 1.5 * (proc % 101) / 100, -0.009);
    glEnd();

    //nacrtam lampicu
    //gde je donji levi cosak
    draw_light(images, texture, -0.18, -0.18, 0.4, 0.4);

    glTranslatef(-start_x, -start_y, 0);

}


/*
	Funkcija: draw_center_of_scene
	Crta elemente u sredistu prozora

	Argumenti:  
		start_x, start_y - pozicija donjeg levog ugla

	Povratna vrednost: nema
 */
static void draw_center_of_scene(float start_x, float start_y)
{
    int j;

    glTranslatef(start_x, start_y, 0);

    //kreiranje poligona koji opisuje vrednost broja obrtaja
    glLineWidth(5);
    glColor3f(.3, .3, .3);

    GLfloat clWhite[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat clTile[] = { 0.29, 0.71, 0.74, 1.0 };
    GLfloat clRed[] = { 1.0, 0.0, 0.0, 1.0 };
    GLfloat clRoad[] = { 0.49, 0.67, 0.64, 1.0 };
    GLfloat clBlack[] = { 0.0, 0.0, 0.0, 1.0 };

    glMaterialfv(GL_FRONT, GL_AMBIENT, clBlack);
    glMaterialfv(GL_FRONT, GL_SPECULAR, clWhite);

    glMaterialfv(GL_FRONT, GL_DIFFUSE, clRoad);
    //centralni
    glBegin(GL_QUADS);
		glNormal3f(0.5, 1, 0);
		glVertex3f(1.0, -1.2, 0.0);
		glVertex3f(-1.0, -1.2, 0.0);
		glNormal3f(0, -1, 0);
		glVertex3f(-1.0, -1.2, -80.0);
		glVertex3f(1.0, -1.2, -80.0);
    glEnd();

    //desni
    glBegin(GL_QUADS);
		glNormal3f(0, 1, 0);
		glVertex3f(2.0, -1.1, 0.0);
		glVertex3f(1.1, -1.1, 0.0);
		glNormal3f(0, -1, 0);
		glVertex3f(1.1, -1.1, -80.0);
		glVertex3f(2.0, -1.1, -80.0);
    glEnd();

    glLineWidth(7);
    glMaterialfv(GL_FRONT, GL_AMBIENT, clTile);
    for (int i = 0; i < 8; i++) {
		char index[2];
		sprintf(index, "%d", 7 - i);
		if (7 - i > 2)
			glRasterPos3f(-2.08 - 0.02 * i, -1.09, 0.1 - 4 * i);
		else
			glRasterPos3f(-2.09 - 0.03 * i, -1.09,  0.1 - 8 * i + 4 * (7 - i + 2));

		glutBitmapString(GLUT_BITMAP_TIMES_ROMAN_24, (const unsigned char *) index);
    }

    //levi
    glMaterialfv(GL_FRONT, GL_AMBIENT, clTile);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, clTile);

    int granica = tacho_val > 2 ? 7 - tacho_val : 3 * (4 - tacho_val);

    glBegin(GL_QUADS);
		for (j = 16; j >= granica; j--) {
			if (j == 1) {
				glMaterialfv(GL_FRONT, GL_AMBIENT, clRed);
				glMaterialfv(GL_FRONT, GL_DIFFUSE, clRed);
			}

			glNormal3f(0, 1, 0);
			glVertex3f(-1.1, -1.09, 0.1 - 4 * j);
			glVertex3f(-2.0, -1.09, 0.1 - 4 * j);
			glNormal3f(0, -1, 0);
			glVertex3f(-2.0, -1.09, 0.1 - 4 * (j + 1));
			glVertex3f(-1.1, -1.09, 0.1 - 4 * (j + 1));
		}
    glEnd();

    glMaterialfv(GL_FRONT, GL_AMBIENT, clBlack);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, clRoad);
    glBegin(GL_QUADS);
		glNormal3f(0, 1, 0);
		glVertex3f(-2.0, -1.10, 0.0);
		glVertex3f(-1.109, -1.10, 0.0);
		glNormal3f(0, -1, 0);
		glVertex3f(-1.109, -1.10, -80.0);
		glVertex3f(-2.0, -1.10, -80.0);
    glEnd();

    glMaterialfv(GL_FRONT, GL_AMBIENT, clTile);

    glTranslatef(-start_x, -start_y, 0);
}
