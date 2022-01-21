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


#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

#define R_VALUE 250
#define G_VALUE 0
#define B_VALUE	250

#define bit_set(val, bit_no) (((val) >> (bit_no)) & 1)

uint8_t  bCenter, iFontSize;
uint16_t  iXPos, iYPos;
char szText[100];

// 'global' variables to store screen info
uint8_t* fbp = 0;
struct fb_var_screeninfo vinfo = { 0 };
struct fb_fix_screeninfo finfo = { 0 };



void put_pixel_RGB565(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{

    // calculate the pixel's byte offset inside the buffer
    // note: x * 2 as every pixel is 2 consecutive bytes
	// 16 bits par pixel
    unsigned int pix_offset = x * 2 + y*vinfo.xres*2;

    // now this is about the same as 'fbp[pix_offset] = value'
    // but a bit more complicated for RGB565
    uint16_t c = b>>3; // b
    c |= ((g>>2)<<5); // g
    c |= ((r>>3)<<11); // r

	//   uint16_t c = ((r / 8) << 11) + ((g / 4) << 5) + (b / 8);
    // or: c = ((r / 8) * 2048) + ((g / 4) * 32) + (b / 8);
    // write 'two bytes at once'
	/*	printf("pix offset:%d , color %d\n",pix_offset, c ); */
    *((unsigned short*)(fbp + pix_offset)) = c;

}



void ShowHelp(void)
{
    printf(
	"fbtext - display text directly onto the framebuffer"
	"usage: ./fbtext <options>\n"
	"valid options:\n\n"
    " --x <Xpos>       X pixel position\n"
	" --y <Ypos>       Y pixel position\n"
	" --f <fontsize factor>   \n"
	" --t <text>   \n"
	"The screen in 480 pixel wide, default character width is 8 pixels.\n"
	"Text to be displayed must be maximum 480/(8*(fontsize factor) characters\n"
    );
}

static void parse_opts(int argc, char *argv[])
{
// set default options
int i = 1;

    bCenter = 0;
	iXPos = iYPos = 0;
	iFontSize = 1;
	memset(szText,0,sizeof(szText));

    while (i < argc)
    {
#ifdef DEBUG_LOG
	printf("i:%d , argv[i]:%s\n",i,argv[i]);
#endif
        /* if it isn't a cmdline option, we're done */
        if (0 != strncmp("--", argv[i], 2))
            break;
        /* GNU-style separator to support files with -- prefix
         * example for a file named "--baz": ./foo --bar -- --baz
         */
        if (0 == strcmp("--", argv[i]))
        {
            i += 1;
            break;
        }
        /* test for each specific flag */
        if (0 == strcmp("--t", argv[i])) {
			strncpy( szText, argv[i+1], sizeof(szText)-1 );   
            i += 2;
        } else if (0 == strcmp("--c", argv[i])) {
            i ++;
            bCenter = 1;
        } else if (0 == strcmp("--x", argv[i])) {
            iXPos = atoi(argv[i+1]); ;
            i += 2;
        } else if (0 == strcmp("--y", argv[i])) {
            iYPos = atoi(argv[i+1]); ;
            i += 2;
        } else if (0 == strcmp("--f", argv[i])) {
            iFontSize  = (uint8_t)atoi(argv[i+1]); ;
            i += 2;

	}  else {
            fprintf(stderr, "Unknown parameter '%s'\n", argv[i]);
            exit(1);
        }
    }
    if (strlen(szText) == 0)
    {
       printf("Must specify a text to display\n\n");
	   ShowHelp();
       exit(1);
    }
} /* parse_opts() */



void DrawChar(char c, uint16_t x, uint16_t y,uint8_t fontsize)
{
    uint8_t i,j;

    // Convert the character to an index
    c = c & 0x7F;
   
    // 'font' is a multidimensional array of [128][char_width]
    // which is really just a 1D array of size 128*char_width.
    const uint8_t* chr = font8x8_basic[(uint8_t)c];
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
				r=R_VALUE; g = G_VALUE ; b=B_VALUE;	
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

#ifdef DEBUG_LOG
	 printf("fontsize:%d\n",fontsize);
#endif
    while (*str) 
		{
        DrawChar(*str++, x, y,fontsize);
        x += CHAR_WIDTH*fontsize;
    }
}



char *centerText(char *pBuffer, const char *text, int fieldWidth) 
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



// application entry point
int main(int argc, char* argv[])
{
	char szBuffer[100];
	memset(szBuffer,0,sizeof(szBuffer));

	parse_opts(argc, argv);

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
#ifdef DEBUG_LOG
	printf("bits per pixel:%d, width:%d, heigth:%d\n",vinfo.bits_per_pixel,vinfo.xres,vinfo.yres); 
#endif

	// Get fixed screen information
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Error reading fixed information.\n");
		close(fbfd);
		return(1);
	}
#ifdef DEBUG_LOG
	printf("mem_size:%d\n",finfo.smem_len);  
#endif


	// map fb to user mem
	fbp = (uint8_t*)mmap(0,	finfo.smem_len,		PROT_READ | PROT_WRITE,		MAP_SHARED,		fbfd,		0);

	if ( fbp == (uint8_t*)-1 )
		return(1);

#ifdef DEBUG_LOG
	printf("displaying:%s on %d:%d with size:%d\n",szText,iXPos,iYPos, iFontSize);  
#endif

	if ( bCenter )
		DrawString(centerText(szBuffer,szText,sizeof(szBuffer)-1),iXPos,iYPos,iFontSize);
	else
		DrawString(szText,iXPos,iYPos,iFontSize);

	// unmap fb file from memory
	munmap(fbp, finfo.smem_len);
	// close fb fil
	close(fbfd);

	return 0;
}
