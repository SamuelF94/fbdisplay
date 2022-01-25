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


uint16_t RGB16(uint8_t r, uint8_t g, uint8_t b )
{
    uint16_t c = b>>3; // b
    c |= ((g>>2)<<5); // g
    c |= ((r>>3)<<11); // r
	return c;
}


void Draw_Bitmap (char* BitmapName,  uint8_t* FrameBuffer)
{
  unsigned int fpos = 0;                      // File position
  if (BitmapName)                         // We have a valid name string
  {
    FILE* fb = fopen(BitmapName, "rb");             // Open the bitmap file
    if (fb > 0)                         // File opened successfully
    {
      size_t br;
      BITMAPFILEHEADER bmpHeader;
      br = fread(&bmpHeader, 1, sizeof(bmpHeader), fb);   // Read the bitmap header
      if (br == sizeof(bmpHeader) && bmpHeader.bfType == 0x4D42) // Check it is BMP file
      {
        BITMAPINFOHEADER bmpInfo;
        fpos = sizeof(bmpHeader);             // File position is at sizeof(bmpHeader)
        br = fread(&bmpInfo, 1, sizeof(bmpInfo), fb);   // Read the bitmap info header
        /** printf("biBitCount:%d, width:%d, heigth:%d\n",bmpInfo.biBitCount, bmpInfo.biWidth,bmpInfo.biHeight); **/

        if (br == sizeof(bmpInfo))              // Check bitmap info read worked
        {
          fpos += sizeof(bmpInfo);            // File position moved sizeof(bmpInfo)
          if (bmpInfo.biSize == 108)
          {
            WIN4XBITMAPINFOHEADER bmpInfo4x;
            br = fread(&bmpInfo4x, 1, sizeof(bmpInfo4x), fb); // Read the bitmap v4 info header exta data
            fpos += sizeof(bmpInfo4x);          // File position moved sizeof(bmpInfo4x)
          }
          if (bmpHeader.bfOffBits > fpos)
          {
            uint8_t b;
            for (int i = 0; i < bmpHeader.bfOffBits - fpos; i++)
              fread(&b, 1, sizeof(b), fb);
            fpos = bmpHeader.bfOffBits;
          }
          /* file positioned at image we must transfer it to screen */
          int y = 0;
          short *destination = (short *)FrameBuffer;
          while (!feof(fb) && y < bmpInfo.biHeight*bmpInfo.biWidth) // While no error and not all line read
          {
            RGB24 theRgb;
            fread(&theRgb, 1, 3, fb);
            uint32_t coordY = vinfo.yres - y/vinfo.xres;
            uint32_t coordX = y%vinfo.xres;
    /**       printf("%03d, %03d\n",coordY, coordX );**/
            destination[coordX + (coordY-1)*480] = RGB16(theRgb.red, theRgb.green, theRgb.blue);
            /** printf("y:%d\n",y ); **/
            y++;
          }
        }
      }
      else perror("Header open failed");
      fclose(fb);
    }
  }
}


void ShowHelp()
{
	printf("usage: fbbmp <filename>\n");
	printf("where filename is an 480x320 24bits bitmap\n");
}


// application entry point
int main(int argc, char* argv[])
{
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

	if ( argc != 2 || (access( argv[1], F_OK ) != 0)  ) 
	{
		ShowHelp();
		exit(0);
	}

	Draw_Bitmap (argv[1], fbp);

	munmap(fbp, finfo.smem_len);
	// close fb fil
	close(fbfd);

	return 0;
}
