//mjpg_streamer -i "input_uvc.so -r 320x240 -q 75 -f 30" -o "output_http.so -p 8090"
#include "buildflags.h"
#include "prob_hough.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#if SDL_USED
 #include <SDL/SDL.h>
 #include <SDL/SDL_gfxPrimitives.h>
#endif

#if USBSTREAM
 #include <string.h>
 #include <jpeglib.h>
 #include <curl/curl.h>
 #include "map.h"
#endif

#if OV7670
 #include <bcm2835.h>
 #define PIN_READY RPI_V2_GPIO_P1_15Me
 #define PIN_REQUEST RPI_V2_GPIO_P1_13

 int readFrame(uint8_t *frameData, uint32_t frameSize, uint32_t options);
#endif



#if USBSTREAM
 struct mem_t {
  uint8_t *memory;
  size_t size;
 };
#endif

const int img_w = IMG_W; /* Should change to use these later, focusing on speed right now */
const int img_h = IMG_H;

int arg_threshold = 36;
int arg_length = 45;
int arg_gap = 3;
uint32_t run=1;

void sigintHandler(int sig_num)
{
	run = 0;
}

int main( int argc, char** argv )
{
	uint32_t divider=6, bpp=2, format=0; /* Options and its defaults */
	uint32_t i, j, k; /* Loop counters */

	char setpoint=0x40;
	int map_i=0;

	img_t in, gauss, out;

	phough_t HT = {0};
	lines_t lv = {0};
	roi_t ROI = {0,0, 640, 480};
	HT.l = &lv;
	in.w = out.w = gauss.w = IMG_W;
	in.h = out.h = gauss.h = IMG_H;
	in.r = out.r = gauss.r = &ROI;

	/* Commandline arguments and options handling */
	int o, index;
	opterr = 0;
	while ((o = getopt (argc, argv, ":p:d:b:t:g:l:")) != -1){
		switch (o)
		{
			case 'p':
				format = atoi(optarg);
				break;
			case 'd':
				divider = atoi(optarg);
				break;
			case 'b':
				bpp = atoi(optarg);
				break;
			case 't':
				arg_threshold = atoi(optarg);
				break;
			case 'l':
				arg_length = atoi(optarg);
				break;
			case 'g':
				arg_gap = atoi(optarg);
				break;
			case ':':
				fprintf(stderr, "No argument specified to option -%c.\n", optopt);
				return -1;
			default:
				fprintf(stderr, "Unexpected option -%c. Is this OK?\n", optopt);
		}
	}

	#if OV7670
		uint8_t tmp_image[width*height*bpp];
		if (!bcm2835_init()){
			fprintf(stderr, "bc2835_init() error!\n");
			return 1;
		}

		/* !Request pin configurations */
		bcm2835_gpio_fsel(PIN_REQUEST, BCM2835_GPIO_FSEL_OUTP);  /* Output */
		bcm2835_gpio_set_pud(PIN_REQUEST, BCM2835_GPIO_PUD_OFF); /* Not pulled by the pi */
		bcm2835_gpio_set(PIN_REQUEST);				 /* Set to high */

		/* !Ready pin configurations */
		bcm2835_gpio_fsel(PIN_READY, BCM2835_GPIO_FSEL_INPT);	 /* Input */
		bcm2835_gpio_set_pud(PIN_READY, BCM2835_GPIO_PUD_OFF);	 /* Not pulled by the pi */

		/* SPI settings */
		bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
		bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
		bcm2835_spi_setClockDivider(divider);
		bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
		bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

		/* Bug:
		 *  bcm2835_spi_begin()/end() "sessions" seem to shift the bits (usually) by one on each call 
		 * Workaround:
		 *  Create and destroy sessions until we receive properly aligned data
		 */
		bcm2835_spi_begin();
		do{
			bcm2835_spi_end();
			bcm2835_spi_begin();
			bcm2835_gpio_clr(PIN_REQUEST); 		/* Signal the STM32 we gant a frame */
			while(bcm2835_gpio_lev(PIN_READY));	/* Wait some some microseconds until it is ready */
			bcm2835_gpio_set(PIN_REQUEST);		/* Signal the STM32 we know it is ready */
			readFrame(image, width*height*bpp, 0);	/* Get the frame */
		}while(image[0] != 0xcc);
		bcm2835_spi_begin();
	#endif
	#if USBSTREAM
		uint8_t raw_image[JPEGALLOC];
		uint8_t rgb_image[IMG_W*IMG_H*3];
		curl_global_init(CURL_GLOBAL_ALL);
	#endif

	uint8_t in_img_data[IMG_W * IMG_H] = {0};
	uint8_t gauss_img_data[IMG_W * IMG_H] = {0};
	uint8_t out_img_data[IMG_W * IMG_H] = {0};
	unsigned int hough_accumulator[((IMG_W + IMG_H) * 2 + 1)*180]; //rho res ignored!
	in.d = in_img_data;
	gauss.d = gauss_img_data;
	out.d = out_img_data;
	HT.accu = hough_accumulator;

	#if SDL_USED
		/* Initialize SDL if needed */
		SDL_Surface *screen = NULL;
		SDL_Color colors[256];
		SDL_Init(SDL_INIT_EVERYTHING);
		#if DRAW_INPUT
			screen = SDL_SetVideoMode(IMG_W, IMG_H, 8, SDL_HWSURFACE);

			for(i=0; i<256; i++){
				colors[i].r = i; colors[i].g = i; colors[i].b = i;
			}

			SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
		#else /* DRAW_OUTPUT*/
			screen = SDL_SetVideoMode(IMG_W, IMG_H, 32, SDL_HWSURFACE);
		#endif

	#endif

/* This is what would be looped */
#if PRINT_TIME
	time_t timer[8];
	time(&timer[0]);
#endif
	signal(SIGINT, sigintHandler);
	printf("Running!\n");

	while(run){
		#if OV7670
			/* Get frame into memory */
			bcm2835_gpio_clr(PIN_REQUEST); 		/* Signal the STM32 we want a frame */
			while(bcm2835_gpio_lev(PIN_READY));	/* Wait some some microseconds until it is ready */
			bcm2835_gpio_set(PIN_REQUEST);		/* Signal the STM32 we know it is ready */
			readFrame(image, width*height*bpp, 0);	/* Get the frame */
			uint16_t *p = (uint16_t*)(&image[5]);
			uint8_t *sp = in_img_data;
			for(i=0; i<IMG_H*IMG_W; i++){
				*sp++ = (uint8_t)(*p++);
			}
		#endif

		#if USBSTREAM
			#if PRINT_TIME >= 2
				time(&timer[1]);
			#endif
			downloadFrame(raw_image, rgb_image, in_img_data);
			#if PRINT_TIME >= 2
				time(&timer[2]);
			#endif
		#endif

		#if DRAW_INPUT
			uint8_t *p = (uint8_t*)(in_img_data);
			uint8_t *sp = (uint8_t*)screen->pixels;
			for(i=0; i<IMG_H*IMG_W; i++){
				*sp++ = (uint8_t)(*p++);

				#if IMG_W==174
					sp += 2;
				#endif
			}
			SDL_Flip(screen);
		#endif

		#if PRINT_TIME >= 2
			time(&timer[3]);
		#endif
		gaussian_noise_reduce(&in, &gauss);	/* Pre-edge detection: some blurring */
		canny_edge_detect(&gauss, &out);	/* Actual edge detection */
		#if PRINT_TIME >= 2
			time(&timer[4]);
		#endif

		#if DRAW_OUTPUT
			uint8_t *p = (uint8_t*)(out_img_data);
			uint32_t *sp = (uint32_t*)screen->pixels;
			for(i=0; i<IMG_H*IMG_W; i++){
				*sp++ = (uint32_t)(0xff000000UL|(*p++)*0x010101UL);

				#if IMG_W==174
					sp += 2;
				#endif
			}
		#endif

		#if PRINT_TIME >= 2
			time(&timer[5]);
		#endif
		init_hough(&HT);
		phough_transform(&out, &HT); /* Transform */
		#if PRINT_TIME >= 2
			time(&timer[6]);
		#endif

		#if DEBUG
			printf("Accumulator info:\n \timage \tw: %d, h: %d\n", HT.img_w, HT.img_h);
			printf("\taccum \tw: %d, h: %d, max: %d\n", HT.w, HT.h, HT.max);
		#endif

		#if DRAW_OUTPUT
			/* Draw all the detected lines in red */
			for(i=0; i<HT.l->count; i++){ 
				lineRGBA(screen, HT.l->l[i].x1, HT.l->l[i].y1, HT.l->l[i].x2, HT.l->l[i].y2, 0xff, 0, 0, 0xff);
			}
		#endif

		int target=10000, target_i=0x232;
		for(i=0; i<HT.l->count; i++){ 
//			HT.l->l[i].len = (int)(sqrt(pow((HT.l->l[i].x2-HT.l->l[i].x1),2) + pow((HT.l->l[i].y2-HT.l->l[i].y1),2)));

//			printf("Line %d x1: %d, x2: %d, y1: %d, y2: %d, s: %d, l: %d\n", i, HT.l->l[i].x1, HT.l->l[i].x2, HT.l->l[i].y1, HT.l->l[i].y2, HT.l->l[i].slope, HT.l->l[i].len);
			int den = (HT.l->l[i].x2-HT.l->l[i].x1);
			int num = (HT.l->l[i].y2-HT.l->l[i].y1);

			if(((HT.l->l[i].y2 + HT.l->l[i].y1) >> 1) < 70){ /* Line is too high*/
				continue;
			}

			if(!den){
				HT.l->l[i].slope = 90;
			}else{
				if((abs(num) << 7)/abs(den) < 89){ /* Slope smaller than 35 */
					continue;
				}
				HT.l->l[i].slope = 57.2956f*atan(num/den);
			}
			HT.l->l[i].d2c = abs(IMG_W - HT.l->l[i].x2 - HT.l->l[i].x1)/2;

			if(HT.l->l[i].d2c < target){
				target = HT.l->l[i].d2c;
				target_i = i;
			}
		}
	
		if(target_i != 0x232){
			i = target_i;
			#if DRAW_OUTPUT
				lineRGBA(screen, HT.l->l[i].x1, HT.l->l[i].y1, HT.l->l[i].x2, HT.l->l[i].y2, 0xff, 0xff, 0, 0xff);
			#endif
//			printf("Line %d x1: %d, x2: %d, y1: %d, y2: %d, s: %d, l: %d\n", i, HT.l->l[i].x1, HT.l->l[i].x2, HT.l->l[i].y1, HT.l->l[i].y2, HT.l->l[i].slope, HT.l->l[i].len);
//			printf("target line d2c: %d\n", HT.l->l[i].d2c);
			map_i = HT.l->l[i].slope <= 0 ? HT.l->l[i].slope*-1 : 180-HT.l->l[i].slope;
//			printf("%d %d\n", HT.l->l[i].slope, map_i);
			setpoint = linemap[map_i];
			//printf("%.2x\n", setpoint);
		}

		#if DRAW_OUTPUT
			SDL_Flip(screen);
		#endif
		#if OUT_NC
			int ret;
			char output[2] = {0x70, 0x60};
			output[0] |= (setpoint >> 4);
			output[1] |= setpoint & 0x0f;
			char cmd[32];
			sprintf(cmd,"./setpoint.sh %.2x %.2x", output[0], output[1]);
			ret = system(cmd);
		#endif
	}

#if PRINT_TIME
	time(&timer[7]);
	#if PRINT_TIME >= 2
		printf("Download - time elapsed: %f\n", difftime(timer[2],timer[1]));
		printf("Edge detection - time elapsed: %f\n", difftime(timer[4],timer[3]));
		printf("Hough Transform - time elapsed: %f\n", difftime(timer[6],timer[5]));
	#endif
	printf("Total - time elapsed: %f\n", difftime(timer[7],timer[0]));
#endif

	printf("Cleaning up\n");
	/* Destroy, clean, end, free, etc */
	#if OV7670
		bcm2835_spi_end();	/* Properly aligned session finishes here, thank you! */
		bcm2835_close();
	#endif

	#if SDL_USED
		SDL_FreeSurface(screen);
		SDL_Quit();
	#endif

	#if USBSTREAM
		curl_global_cleanup();
	#endif

 return 0;
}

#if OV7670
 int readFrame(uint8_t *frameData, uint32_t frameSize, uint32_t options)
 {
	bcm2835_spi_transfern((char *)frameData, frameSize);

	if(options){	
		unsigned int i;
		for(i = 0; i<frameSize; i++){
			printf("%02X ", *(frameData++));
			if(i%options == options-1){
				printf("\n");
			}
		}
		printf("\n");
	}

  return 0;
 }
#endif

#if USBSTREAM
 static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
 {
  size_t realsize = size * nmemb;
  struct mem_t *mem = (struct mem_t *)userp;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
 }

 int jpeg_inflate(unsigned char *jpegbuff, size_t jpegsize, uint8_t *rgbbuff)
 {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	unsigned char row_static[3*IMG_W];
	unsigned long location = 0;
	int i = 0;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, jpegbuff, jpegsize);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	row_pointer[0] = row_static;

	while (cinfo.output_scanline < cinfo.image_height) {
		jpeg_read_scanlines( &cinfo, row_pointer, 1 );
		for (i=0; i<cinfo.image_width*cinfo.num_components;i++) {
			rgbbuff[location++] = row_pointer[0][i];
		}
	}
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return 1;
 }

 int downloadFrame(uint8_t *jpeg_data, uint8_t *rgb_data, uint8_t *grayscale_data)
 {
	CURL *curl_handle;
	CURLcode res;
	struct mem_t block;
	block.memory = jpeg_data;
	block.size = 0;

	curl_handle = curl_easy_init();

	#if LOCALSTREAM
		curl_easy_setopt(curl_handle, CURLOPT_URL, "http://127.0.0.1/?action=snapshot");
	#else
		curl_easy_setopt(curl_handle, CURLOPT_URL, "http://172.16.100.76/?action=snapshot");
	#endif

	curl_easy_setopt(curl_handle, CURLOPT_PORT , 8090);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&block);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	res = curl_easy_perform(curl_handle);
	#ifndef BRIGHTSIDE
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
	}
	else { /* HTTP success */
		if (jpeg_inflate((unsigned char*)block.memory, block.size, rgb_data) > 0) {
	#else
		jpeg_inflate((unsigned char*)block.memory, block.size, rgb_data);
	#endif
		/* JPEG inflated, now get grayscale */
			uint32_t i;
			for(i=0; i<IMG_H*IMG_W; i++){
			/* Store into the final buffer */
				#if GRAY_AVG
					*grayscale_data++ = (*rgb_data + *(rgb_data+1) + *(rgb_data+2))/3;
				#endif
				#if GRAY_GRN
					*grayscale_data++ = *(rgb_data+1);
				#endif
				rgb_data += 3;
			}
	#ifndef BRIGHTSIDE
		}
	}
	#endif

	/* cleanup curl stuff */
	curl_easy_cleanup(curl_handle);

	return 0;
 }

#endif
