/*
 * main.c
 *
 *
 *  Created on: 14.08.2013
 *      Author: alexs
 *
 * Modified ON : 14.08.2018
 *     Author: Daniel Perron
 *     Modification: Remove bcm2835 library
 *                   Just use standard Linux SPI driver
 */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sched.h>
#include <signal.h>
#include "rfid.h"
#include "config.h"
#include "main.h"


int spi_handle=-1;
int32_t spi_speed=10000000L;


// I/O access
// we will use /dev/gpiomem
#define USE_GPIOMEM
// I/O handle
int  mem_fd;  //I/O handle
void *gpio_map;  // map gpio
// map gpio
volatile unsigned long *GPIO; // DWORD GPIO access




uint8_t HW_init(uint32_t spi_speed, uint8_t gpio);
void setup_io();
void usage(char *);
char *KeyA="Test01";
char *KeyB="Test10";
uint8_t debug=0;


int main(int argc, char *argv[]) {

	uid_t uid;
	uint8_t SN[10];
	uint16_t CType=0;
	uint8_t SN_len=0;
	char status;
	int tmp,i;

	char str[255];
	char *p;
	char sn_str[23];
	pid_t child;
	int max_page=0;
	uint8_t page_step=0;

	FILE * fmem_str;
	char save_mem=0;
	char fmem_path[255];
	uint8_t use_gpio=0;
	uint8_t gpio=255;

	if (argc>1) {
		if (strcmp(argv[1],"-d")==0) {debug=1;
		printf("Debug mode.\n");
		}else{
			usage(argv[0]);
			exit(1);
		}
	}

	if (open_config_file(config_file)!=0) {
		if (debug) {fprintf(stderr,"Can't open config file!");}
		else{syslog(LOG_DAEMON|LOG_ERR,"Can't open config file!");}
		return 1;
	}
	if (find_config_param("GPIO=",str,sizeof(str)-1,1)==1) {
		gpio=(uint8_t)strtol(str,NULL,10);
		if (gpio<28) {
			use_gpio=1;
		} else {
			gpio=255;
			use_gpio=0;
		}
	}
	if (find_config_param("SPI_SPEED=",str,sizeof(str)-1,1)==1) {
		spi_speed=(uint32_t)strtoul(str,NULL,10);
		if (spi_speed>10000L) spi_speed=10000L;
		if (spi_speed<100L) spi_speed=100L;
	}

	if (HW_init(spi_speed,gpio)) return 1; // Если не удалось инициализировать RC522 выходим.
	if (read_conf_uid(&uid)!=0) return 1;
        setup_io();
        if (use_gpio)  OUT_GPIO(gpio);
	setuid(uid);


	InitRc522();

	if (find_config_param("NEW_TAG_PATH=",fmem_path,sizeof(fmem_path)-1,0)) {
		save_mem=1;
		if (fmem_path[strlen(fmem_path)-1]!='/') {
			sprintf(&fmem_path[strlen(fmem_path)],"/");
			if (strlen(fmem_path)>=240) {
				if (debug) {fprintf(stderr,"Too long path for tag dump files!");}
				else{syslog(LOG_DAEMON|LOG_ERR,"Too long path for tag dump files!");}
				return 1;
			}
		}
	}

	for (;;) {
		status= find_tag(&CType);
		if (status==TAG_NOTAG) {
			usleep(200000);
			continue;
		}else if ((status!=TAG_OK)&&(status!=TAG_COLLISION)) {continue;}

		if (select_tag_sn(SN,&SN_len)!=TAG_OK) {continue;}

		p=sn_str;
		*(p++)='[';
		for (tmp=0;tmp<SN_len;tmp++) {
			sprintf(p,"%02x",SN[tmp]);
			p+=2;
		}
		//for debugging
		if (debug) {
		*p=0;
		fprintf(stderr,"Type: %04x, Serial: %s\n",CType,&sn_str[1]);
		}
		*(p++)=']';
		*(p++)=0;

//		if (use_gpio) bcm2835_gpio_write(gpio, HIGH);
                if (use_gpio) GPIO_SET= 1<<gpio;
		//ищем SN в конфиге
		if (find_config_param(sn_str,str,sizeof(str),1)>0) {
			child=fork();
			if (child==0) {
				fclose(stdin);
				freopen("","w",stdout);
				freopen("","w",stderr);
				execl("/bin/sh","sh","-c",str,NULL);
			} else if (child>0) {
				i=6000;
				do {
					usleep(10000);
					tmp=wait3(NULL,WNOHANG,NULL);
					i--;
				} while (i>0 && tmp!=child);

				if (tmp!=child) {
					kill(child,SIGKILL);
					wait3(NULL,0,NULL);
					if (debug) {fprintf(stderr,"Killed\n");}
				}else {
					if (debug) {fprintf(stderr,"Exit\n");}
				}
			}else{
				if (debug) {fprintf(stderr,"Can't run child process! (%s %s)\n",sn_str,str);}
				else{syslog(LOG_DAEMON|LOG_ERR,"Can't run child process! (%s %s)\n",sn_str,str);}
			}

		}else{

			if (debug) {fprintf(stderr,"New tag: type=%04x SNlen=%d SN=%s\n",CType,SN_len,sn_str);}
				else{syslog(LOG_DAEMON|LOG_INFO,"New tag: type=%04x SNlen=%d SN=%s\n",CType,SN_len,sn_str);}

			if (save_mem) {
				switch (CType) {
				case 0x4400:
					max_page=0x0f;
					page_step=4;
					break;
				case 0x0400:
					PcdHalt();
//					if (use_gpio) bcm2835_gpio_write(gpio, LOW);
                                        if (use_gpio) GPIO_CLR=1<<gpio;
					continue;
					max_page=0x3f;
					page_step=1;
					break;
				default:
					break;
				}
				p=str;
				sprintf(p,"%s",fmem_path);
				p+=strlen(p);
				for (tmp=0;tmp<SN_len;tmp++) {
					sprintf(p,"%02x",SN[tmp]);
					p+=2;
				}
				sprintf(p,".txt");
				if ((fmem_str=fopen(str,"r"))!=NULL) {
					fclose(fmem_str);
					PcdHalt();
//					if (use_gpio) bcm2835_gpio_write(gpio, LOW);
                                        if (use_gpio) GPIO_CLR=1<<gpio;
					continue;
				}
				if ((fmem_str=fopen(str,"w"))==NULL) {
					syslog(LOG_DAEMON|LOG_ERR,"Cant open file for write: %s",str);
					PcdHalt();
//					if (use_gpio) bcm2835_gpio_write(gpio, LOW);
                                        if (use_gpio) GPIO_CLR = 1 << gpio;
					continue;
				}
				for (i=0;i<max_page;i+=page_step) {
					read_tag_str(i,str);
					fprintf(fmem_str,"%02x: %s\n",i,str);
				}
				fclose(fmem_str);
			}
		}
		PcdHalt();
//		if (use_gpio) bcm2835_gpio_write(gpio, LOW);
                if (use_gpio) GPIO_CLR=1<<gpio;
	}

//	bcm2835_spi_end();
//	bcm2835_close();
	close_config_file();
	return 0;

}



uint8_t HW_init(uint32_t spi_speed, uint8_t gpio) {
        int status_value = -1;
        unsigned char spi_mode=0;
        unsigned char spi_bitsPerWord=8;
        unsigned long  _spi_speed = spi_speed * 1000L;

        spi_handle = open("/dev/spidev0.0",O_RDWR);

       if(spi_handle < 0)
         {
           perror("Error - Could not open SPI device");
           exit(1);
         }


      status_value = ioctl(spi_handle, SPI_IOC_WR_MODE, &spi_mode);
      if(status_value < 0)
      {
        perror("Could not set SPIMode (WR)...ioctl fail");
        exit(1);
      }

      status_value = ioctl(spi_handle, SPI_IOC_RD_MODE, &spi_mode);
      if(status_value < 0)
      {
       perror("Could not set SPIMode (RD)...ioctl fail");
       exit(1);
      }

      status_value = ioctl(spi_handle, SPI_IOC_WR_BITS_PER_WORD, &spi_bitsPerWord);
      if(status_value < 0)
      {
      perror("Could not set SPI bitsPerWord (WR)...ioctl fail");
      exit(1);
      }

      status_value = ioctl(spi_handle, SPI_IOC_RD_BITS_PER_WORD, &spi_bitsPerWord);
      if(status_value < 0)
    {
      perror("Could not set SPI bitsPerWord(RD)...ioctl fail");
      exit(1);
    }

    status_value = ioctl(spi_handle, SPI_IOC_WR_MAX_SPEED_HZ, &_spi_speed);
    if(status_value < 0)
    {
      perror("Could not set SPI speed (WR)...ioctl fail");
      exit(1);
    }

    status_value = ioctl(spi_handle, SPI_IOC_RD_MAX_SPEED_HZ, &_spi_speed);
    if(status_value < 0)
    {
      perror("Could not set SPI speed (RD)...ioctl fail");
      exit(1);
    }
    return(status_value);
}


void my_spi_transfer(unsigned char *txbuff, unsigned char *rxbuff, int len)
{
  int ret;
  struct spi_ioc_transfer spi_tr = {
                .tx_buf = (unsigned long)  txbuff,
                .rx_buf = (unsigned long)  rxbuff,
                .len = len,
                .delay_usecs = 0,
                .speed_hz = spi_speed * 1000,  // speed in KHz to Hz
                .bits_per_word = 8,
              };
        ret = ioctl(spi_handle,SPI_IOC_MESSAGE(1), &spi_tr);
        if(ret < 1)
         {
          perror("Can't send spi message\n");
          close(spi_handle);
          exit(2);
        }
}

void usage(char * str) {
	printf("Usage:\n%s [options]\n\nOptions:\n -d\t Debug mode\n\n",str);
}

void setup_io()
{
 int handle;
  int count;
  struct{
  unsigned   long  V1,V2,V3;
  }ranges;

unsigned long BCM2708_PERI_BASE=0x20000000;



#ifdef USE_GPIOMEM
   /* open /dev/mem */
   if ((mem_fd = open("/dev/gpiomem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/gpiomem \n");
      exit(-1);
   }
#else
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }
#endif

  // read /proc/device-tree/soc/ranges
  // to check if we have the GPIO at 0x20000000 or 0x3F000000

#define Swap4Bytes(val) \
 ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
   (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )


  handle =  open("/proc/device-tree/soc/ranges" ,  O_RDONLY);

  if(handle >=0)
   {
     count = read(handle,&ranges,12);
     if(count == 12)
       BCM2708_PERI_BASE=Swap4Bytes(ranges.V2);
     close(handle);
   }

//   printf("BCM GPIO BASE= %lx\n",BCM2708_PERI_BASE);


#ifdef USE_GPIOMEM
  /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      0xB4,             //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      0                 //Offset to GPIO peripheral
   );
#else
   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );
#endif


   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   GPIO = (volatile unsigned long *)gpio_map;




} // setup_io

