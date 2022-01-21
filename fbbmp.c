#define _DEFAULT_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h> 
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "font.h"


#define IN  0
#define OUT 1

#define LOW  0
#define HIGH 1

#define PIN  26 /* P1-18 */


const char displayFile[] = "/var/www/html/display.txt";
const char actionFile[] = "/var/www/html/action.txt";


/**
#include <signal.h>
#include <pthread.h>
**/

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 




static unsigned short def_r[] = 
    { 0,   0,   0,   0, 172, 172, 172, 172,  
     84,  84,  84,  84, 255, 255, 255, 255};
static unsigned short def_g[] = 
    { 0,   0, 172, 172,   0,   0,  84, 172,  
     84,  84, 255, 255,  84,  84, 255, 255};
static unsigned short def_b[] = 
    { 0, 172,   0, 172,   0, 172,   0, 172,  
     84, 255,  84, 255,  84, 255,  84, 255};


/*--------------------------------------------------------------------------}
{                        BITMAP FILE HEADER DEFINITION                      }
{--------------------------------------------------------------------------*/
#pragma pack(1)									// Must be packed as read from file
typedef struct tagBITMAPFILEHEADER {
	uint16_t  bfType; 							// Bitmap type should be "BM"
	uint32_t  bfSize; 							// Bitmap size in bytes
	uint16_t  bfReserved1; 						// reserved short1
	uint16_t  bfReserved2; 						// reserved short2
	uint32_t  bfOffBits; 						// Offset to bmp data
} BITMAPFILEHEADER, * LPBITMAPFILEHEADER, * PBITMAPFILEHEADER;
#pragma pack()

/*--------------------------------------------------------------------------}
{                    BITMAP FILE INFO HEADER DEFINITION						}
{--------------------------------------------------------------------------*/
#pragma pack(1)									// Must be packed as read from file
typedef struct tagBITMAPINFOHEADER {
	uint32_t biSize; 							// Bitmap file size
	uint32_t biWidth; 							// Bitmap width
	uint32_t biHeight;							// Bitmap height
	uint16_t biPlanes; 							// Planes in image
	uint16_t biBitCount; 						// Bits per byte
	uint32_t biCompression; 					// Compression technology
	uint32_t biSizeImage; 						// Image byte size
	uint32_t biXPelsPerMeter; 					// Pixels per x meter
	uint32_t biYPelsPerMeter; 					// Pixels per y meter
	uint32_t biClrUsed; 						// Number of color indexes needed
	uint32_t biClrImportant; 					// Min colours needed
} BITMAPINFOHEADER, * PBITMAPINFOHEADER;
#pragma pack()

/*--------------------------------------------------------------------------}
{				  BITMAP VER 4 FILE INFO HEADER DEFINITION					}
{--------------------------------------------------------------------------*/
#pragma pack(1)
typedef struct tagWIN4XBITMAPINFOHEADER
{
	uint32_t RedMask;       /* Mask identifying bits of red component */
	uint32_t GreenMask;     /* Mask identifying bits of green component */
	uint32_t BlueMask;      /* Mask identifying bits of blue component */
	uint32_t AlphaMask;     /* Mask identifying bits of alpha component */
	uint32_t CSType;        /* Color space type */
	uint32_t RedX;          /* X coordinate of red endpoint */
	uint32_t RedY;          /* Y coordinate of red endpoint */
	uint32_t RedZ;          /* Z coordinate of red endpoint */
	uint32_t GreenX;        /* X coordinate of green endpoint */
	uint32_t GreenY;        /* Y coordinate of green endpoint */
	uint32_t GreenZ;        /* Z coordinate of green endpoint */
	uint32_t BlueX;         /* X coordinate of blue endpoint */
	uint32_t BlueY;         /* Y coordinate of blue endpoint */
	uint32_t BlueZ;         /* Z coordinate of blue endpoint */
	uint32_t GammaRed;      /* Gamma red coordinate scale value */
	uint32_t GammaGreen;    /* Gamma green coordinate scale value */
	uint32_t GammaBlue;     /* Gamma blue coordinate scale value */
} WIN4XBITMAPINFOHEADER;


typedef struct tagRGB24
{
	uint8_t blue;
	uint8_t green;
	uint8_t red;
} RGB24;


typedef struct tagRGB565
{
	uint16_t b:5;
	uint16_t g:6;
	uint16_t r:5;
} RGB565;

#pragma pack()

// 'global' variables to store screen info
uint8_t* fbp = 0;
struct fb_var_screeninfo vinfo = { 0 };
struct fb_fix_screeninfo finfo = { 0 };




static int
GPIOExport(int pin)
{
#define BUFFER_MAX 3
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int
GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int
GPIODirection(int pin, int dir)
{
	static const char s_directions_str[]  = "in\0out";

#define DIRECTION_MAX 35
	char path[DIRECTION_MAX];
	int fd;

	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return(-1);
	}

	if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}

	close(fd);
	return(0);
}

static int
GPIORead(int pin)
{
#define VALUE_MAX 30
	char path[VALUE_MAX];
	char value_str[3];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return(-1);
	}

	if (-1 == read(fd, value_str, 3)) {
		fprintf(stderr, "Failed to read value!\n");
		return(-1);
	}

	close(fd);

	return(atoi(value_str));
}

static int
GPIOWrite(int pin, int value)
{
	static const char s_values_str[] = "01";

	char path[VALUE_MAX];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return(-1);
	}

	if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
		fprintf(stderr, "Failed to write value!\n");
		return(-1);
	}

	close(fd);
	return(0);
}


static void SwapBytes(uint16_t *color) 
{
    uint8_t temp = *color >> 8;
    *color = (*color << 8) | temp;
}


void put_pixel_RGB565(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{

    // calculate the pixel's byte offset inside the buffer
    // note: x * 2 as every pixel is 2 consecutive bytes
	// 16 bits par pixel
    unsigned int pix_offset = x * 2 + y * 480*2;

    // now this is about the same as 'fbp[pix_offset] = value'
    // but a bit more complicated for RGB565
    uint16_t c = ((r / 8) << 11) + ((g / 4) << 5) + (b / 8);
    // or: c = ((r / 8) * 2048) + ((g / 4) * 32) + (b / 8);
    // write 'two bytes at once'
	/*	printf("pix offset:%d , color %d\n",pix_offset, c ); */
    *((unsigned short*)(fbp + pix_offset)) = c;

}



#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

#define bit_set(val, bit_no) (((val) >> (bit_no)) & 1)

void DrawChar(char c, uint16_t x, uint16_t y,uint8_t fontsize)
{
    uint8_t i,j;

    // Convert the character to an index
/*		printf("%c -",c); */
    c = c & 0x7F;
   
/*		printf("%d\r\n",c); */

    // 'font' is a multidimensional array of [96][char_width]
    // which is really just a 1D array of size 96*char_width.
    const uint8_t* chr = font8x8_basic[c];
		uint8_t r,g,b;
    // Draw pixels
    for (j=0; j<CHAR_WIDTH; j++) 
		{
				uint8_t val = chr[j];
				uint8_t isize, jsize;
        for (i=0; i<CHAR_HEIGHT; i++) 
				{
            if (bit_set(val,i))
						{
							r=250; g = 0 ; b=250;	
						}
						else
						{
							r=0; g = 0 ; b=0;
						}
						for ( jsize = 0 ; jsize < fontsize; jsize++)
						{
							for ( isize = 0 ; isize < fontsize; isize++)
		            put_pixel_RGB565(x+i*fontsize+jsize, y+j*fontsize+isize, r,g,b);
						}
        }
    }
}



void DrawString(const char* str, uint16_t x, uint16_t y, uint8_t fontsize)
{
    while (*str) 
		{
        DrawChar(*str++, x, y,fontsize);
        x += CHAR_WIDTH*fontsize;
    }
}


/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}


time_t getFileModificationTime(const char *path) 
{
    struct stat attr;
    stat(path, &attr);
/**    printf("Last modified time: %s", localtime(&attr.st_mtime)); **/
		return attr.st_mtime;
}


char *centerText(char *pBuffer, char *text, int fieldWidth) 
{
		if ( strlen(text) < fieldWidth )
		{
	    int padlen = (fieldWidth - strlen(text)) / 2;
  	  snprintf(pBuffer,fieldWidth,"%*s%s%*s\n", padlen, "", text, padlen, "");
		}
		else 
  	  snprintf(pBuffer,fieldWidth,"%s",text);
		return pBuffer;
} 


void Reboot()
{
	/* system("sudo shutdown now"); */
	printf("shutdown\n");
}


// application entry point
int main(int argc, char* argv[])
{
	char szDisplay[15];
	int iRepeat = 1, iboucle;
	// Open the file for reading and writing
	int fbfd = open("/dev/fb1", O_RDWR);
	if (fbfd == -1) {
		perror("Error: cannot open framebuffer device.\n");
		return(1);
	}

	// Get variable screen information
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
		perror("Error reading variable information.\n");
		close(fbfd);
		return(1);
	}
	printf("bits per pixel:%d, width:%d, heigth:%d\n",vinfo.bits_per_pixel,vinfo.xres,vinfo.yres); 


	// Get fixed screen information
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Error reading fixed information.\n");
		close(fbfd);
		return(1);
	}
/**	printf("mem_size:%d\n",finfo.smem_len);  **/


	// map fb to user mem
	fbp = (uint8_t*)mmap(0,	finfo.smem_len,		PROT_READ | PROT_WRITE,		MAP_SHARED,		fbfd,		0);

	if ( fbp == (uint8_t*)-1 )
		return(1);

	time_t fileTimeDisplay = getFileModificationTime(displayFile);
	time_t fileTimeAction = getFileModificationTime(actionFile);


	if ( -1 == GPIOExport(PIN))
	{
		printf("Unable to  use gpio\n");
	}

	if(  -1 == GPIODirection(PIN, IN))
	{
		printf("Unable to set pin direction\n");
	}





	for (;;)
	{
		
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		sprintf(szDisplay,"     %02d:%02d      ", tm.tm_hour, tm.tm_min, tm.tm_sec);
		DrawString(szDisplay,0,50,4);
		time_t currentfileTimeDisplay = getFileModificationTime(displayFile);
		time_t currentfileTimeAction = getFileModificationTime(actionFile);
		if ( GPIORead(PIN) == HIGH )
			Reboot();
 		if ( currentfileTimeDisplay > fileTimeDisplay )
		{
			fileTimeDisplay = currentfileTimeDisplay;
			FILE * pF =  fopen( displayFile,"r" );
			if ( pF != NULL )
			{
				char szBuffer[200], szBuffer2[200];
				memset(szBuffer,0,sizeof(szBuffer));
				memset(szBuffer2,0,sizeof(szBuffer2));
				fread(szBuffer, 50,1,pF);
				fclose(pF);
				DrawString(centerText(szBuffer2,szBuffer,30),10,250,2);
			}
		}
		if ( GPIORead(PIN) == HIGH )
			Reboot();
 		if ( currentfileTimeAction > fileTimeAction )
		{
			fileTimeAction = currentfileTimeAction;
			FILE * pF =  fopen( actionFile,"r" );
			if ( pF != NULL )
			{
				char szBuffer[500];
				memset(szBuffer,0,sizeof(szBuffer));
				fread(szBuffer, 450,1,pF);
				fclose(pF);
				system( szBuffer );
			}
		}
		if ( GPIORead(26) == HIGH )
			Reboot();
		msleep(500);
	}


	// unmap fb file from memory
	munmap(fbp, finfo.smem_len);
	// close fb fil
	close(fbfd);

	return 0;
}
