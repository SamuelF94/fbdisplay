#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

// 'global' variables to store screen info
uint8_t* fbp = 0;
struct fb_var_screeninfo vinfo = { 0 };
struct fb_fix_screeninfo finfo = { 0 };

#define FRAMEBUFFER "/dev/fb1"


// application entry point
int main(int argc, char* argv[])
{
	// Open the file for reading and writing
	int fbfd = open(FRAMEBUFFER, O_RDWR);
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

	// Get fixed screen information
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		perror("Error reading fixed information.\n");
		close(fbfd);
		return(1);
	}
	// map fb to user mem
	fbp = (uint8_t*)mmap(0,	finfo.smem_len,		PROT_READ | PROT_WRITE,		MAP_SHARED,		fbfd,		0);

	if ( fbp == (uint8_t*)-1 )
		return(1);

	memset(fbp,0,finfo.smem_len); 

	munmap(fbp, finfo.smem_len);
	// close fb fil
	close(fbfd);

	return 0;
}
