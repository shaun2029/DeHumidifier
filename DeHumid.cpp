#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <bcm2835.h>
#include <time.h>
#include <string.h>
#include "Adafruit_BME280.h"

#define SEALEVELPRESSURE_HPA (1013.25)

#define GPIO17 RPI_V2_GPIO_P1_11
#define GPIO22 RPI_V2_GPIO_P1_15
#define GPIO23 RPI_V2_GPIO_P1_16
#define GPIO27 RPI_V2_GPIO_P1_13
#define GPIO24 RPI_V2_GPIO_P1_18
#define GPIO25 RPI_V2_GPIO_P1_22

/*
 * "OUT OF THE BOX: Plug the Pi Transmitter board into the Raspberry Pi"
 * "GPIO pin-header ensuring correct polarity and pin alignment."
 * ""
 * "The sockets will need to be inserted into separate mains wall sockets."
 * "with a physical separation of at least 2 metres to ensure they don't"
 * "interfere with each other. Do not put into a single extension lead."
 * ""
 * "For proper set up the sockets should be in their factory state with"
 * "the red led flashing at 1 second intervals. If this is not the case for"
 * "either socket, press and hold the green button on the front of the unit"
 * "for 5 seconds or more until the red light flashes slowly."
 * ""
 * "A socket in learning mode will be listening for a control code to be"
 * "sent from a transmitter. A socket can pair with up to 2 transmitters"
 * "and will accept the following code pairs"
 * ""
 * "0011 and 1011 all sockets ON and OFF"
 * "1111 and 0111 socket 1 ON and OFF"
 * "1110 and 0110 socket 2 ON and OFF"
 * "1101 and 0101 socket 3 ON and OFF"
 * "1100 and 0100 socket 4 ON and OFF"
 * ""
 * "A socket in learning mode should accept the first code it receives"
 * "If you wish the sockets to react to different codes, plug in and"
 * "program first one socket then the other using this program."
 * ""
 * "When the code is accepted you will see the red lamp on the socket"
 * "flash quickly then extinguish"
 * ""
*/
int InitPlugs(void) {

    if (!bcm2835_init())
	{
   	 	perror("Failed to map the physical GPIO registers into the virtual memory space.\n");
    	return -1;
    }
        
	/* Select the GPIO pins used for the encoder K0-K3 data inputs */
    bcm2835_gpio_fsel(GPIO17, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(GPIO22, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(GPIO23, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(GPIO27, BCM2835_GPIO_FSEL_OUTP);


    /* Select the signal to select ASK/FSK */
    bcm2835_gpio_fsel(GPIO24, BCM2835_GPIO_FSEL_OUTP);

    /* Select the signal used to enable/disable the modulator */
    bcm2835_gpio_fsel(GPIO25, BCM2835_GPIO_FSEL_OUTP);

    /* Disable the modulator by setting CE pin low */
    bcm2835_gpio_write(GPIO25, LOW);

    /* Set the modulator to ASK for On Off Keying 
     * by setting MODSEL pin low. */
    bcm2835_gpio_write(GPIO24, LOW);

    /* Initialise K0-K3 inputs of the encoder to 0000 */
    bcm2835_gpio_write(GPIO17, LOW);
    bcm2835_gpio_write(GPIO22, LOW);
    bcm2835_gpio_write(GPIO23, LOW);
    bcm2835_gpio_write(GPIO27, LOW);
    
    return 0;
}

/* "0011 and 1011 all sockets ON and OFF"
 * "1111 and 0111 socket 1 ON and OFF"
 * "1110 and 0110 socket 2 ON and OFF"
 * "1101 and 0101 socket 3 ON and OFF"
 * "1100 and 0100 socket 4 ON and OFF" */

int SetPlugState(int number, bool state)
{
    for (int i=0; i<4; i++) {
        switch (number) {
            case 1:
                if (state) {
                    bcm2835_gpio_write(GPIO17, HIGH);
                    bcm2835_gpio_write(GPIO22, HIGH);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, HIGH);
                }
                else {
                    bcm2835_gpio_write(GPIO17, HIGH);
                    bcm2835_gpio_write(GPIO22, HIGH);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, LOW);
                }
                break;
                
            case 2:
                if (state) {
                    bcm2835_gpio_write(GPIO17, LOW);
                    bcm2835_gpio_write(GPIO22, HIGH);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, HIGH);
                }
                else {
                    bcm2835_gpio_write(GPIO17, LOW);
                    bcm2835_gpio_write(GPIO22, HIGH);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, LOW);
                }
                break;
                
            case 3:
                if (state) {
                    bcm2835_gpio_write(GPIO17, HIGH);
                    bcm2835_gpio_write(GPIO22, LOW);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, HIGH);
                }
                else {
                    bcm2835_gpio_write(GPIO17, HIGH);
                    bcm2835_gpio_write(GPIO22, LOW);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, LOW);
                }
                break;
                
            case 4:
                if (state) {
                    bcm2835_gpio_write(GPIO17, LOW);
                    bcm2835_gpio_write(GPIO22, LOW);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, HIGH);
                }
                else {
                    bcm2835_gpio_write(GPIO17, LOW);
                    bcm2835_gpio_write(GPIO22, LOW);
                    bcm2835_gpio_write(GPIO23, HIGH);
                    bcm2835_gpio_write(GPIO27, LOW);
                }
                break;
                
            default:
               if (state) {
                    bcm2835_gpio_write(GPIO17, HIGH);
                    bcm2835_gpio_write(GPIO22, HIGH);
                    bcm2835_gpio_write(GPIO23, LOW);
                    bcm2835_gpio_write(GPIO27, LOW);
                }
                else {
                    bcm2835_gpio_write(GPIO17, HIGH);
                    bcm2835_gpio_write(GPIO22, HIGH);
                    bcm2835_gpio_write(GPIO23, LOW);
                    bcm2835_gpio_write(GPIO27, HIGH);
                }     
                break;   
        }
        
        /* let it settle, encoder requires this time. */
        usleep(100000);
	    /* Enable the modulator */
	    bcm2835_gpio_write(GPIO25, HIGH);
	    /* keep enabled for a period */
        usleep(250000);
	    /* Disable the modulator */
	    bcm2835_gpio_write(GPIO25, LOW);    
    }
    
	return 0;	
} 
 
const char *
parse_date (const char *input, struct tm *tm)
{
  const char *cp;

  /* First clear the result structure.  */
  memset (tm, '\0', sizeof (*tm));

  /* Try the ISO format first.  */
  cp = strptime (input, "%H:%M", tm);

  return cp;
}
 
int main(int argc, const char * argv[])
{
    Adafruit_BME280 bme;
    bool state = false;
    double maxHumidity = 50;
    double minHumidity = 45;
    struct tm timeStart, timeStop;
    
    int onTime = 0;
    
    timeStart.tm_hour = 23;
    timeStart.tm_min = 30;
    timeStop.tm_hour = 6;
    timeStop.tm_min = 30;

 	fprintf(stderr, "DeHumid Version: 1.0.0\n");
 	fprintf(stderr, "Copyright Shaun Simpson 2016\n\n");

    if (argc > 2) {
		minHumidity = atof(argv[1]);
		maxHumidity = atof(argv[2]);
    }
    
    if (argc > 4) {
        struct tm time;
        if (parse_date(argv[3], &time) == NULL) {
           	fprintf(stderr, "Failed to parse start time: %s\n", argv[3]);
        }
        else {
            timeStart = time;
        }
        
        if (parse_date(argv[4], &time) == NULL) {
           	fprintf(stderr, "Failed to parse stop time: %s\n", argv[4]);
        }
        else {
            timeStop = time;
        }
        
    }

	printf("Min humidity set to %.1f\n", minHumidity);
	printf("Max humidity set to %.1f\n", maxHumidity);
    fprintf(stdout, "Start time set to %2d:%2d\n", timeStart.tm_hour, timeStart.tm_min);
    fprintf(stdout, "Stop time set to %2d:%2d\n\n", timeStop.tm_hour, timeStop.tm_min);
    
	if (InitPlugs())
	{
       	 	perror("Failed to initialise plug GPIO control.\n");
        	return -1;
   	}

    SetPlugState(1, false);   
    
    if (bme.begin(BME280_ADDRESS, "/dev/i2c-1")) {
        /* Give sensor some time. */
        usleep(100000);
        SetPlugState(1, state);   
    
        while (true) {
            time_t rawtime;
            struct tm * timeinfo;

            time (&rawtime);
            timeinfo = localtime (&rawtime);
            fprintf(stdout, "%2d:%2d: ", timeinfo->tm_hour, timeinfo->tm_min);

            fprintf(stdout, "Temperature = %.2f *C\t", bme.readTemperature());
            fprintf(stdout, "Pressure = %.2f hPa\t", bme.readPressure() / 100.0F);
            fprintf(stdout, "Approx. Altitude = %.2f m\t", bme.readAltitude(SEALEVELPRESSURE_HPA));
            double humidity = bme.readHumidity();
            fprintf(stdout, "Humidity = %.2f %\n", humidity);
            
            int tMin = timeinfo->tm_hour * 60 + timeinfo->tm_min;
           
            /* Control if time between Start and Stop */
            if ((tMin >= timeStart.tm_hour*60+timeStart.tm_min) || (tMin < timeStop.tm_hour*60+timeStop.tm_min)) {
                if (humidity > maxHumidity) {
                    if (state != true) {
                        state = true;
                        SetPlugState(1, state);   
                        fprintf(stdout, "%2d:%2d: Turned ON\n", timeinfo->tm_hour, timeinfo->tm_min);
                    }
                }
                else if (humidity < minHumidity) {
                    if (state != false) {
                        state = false;
                        SetPlugState(1, state);   
                        fprintf(stdout, "%2d:%2d: Turned OFF\n", timeinfo->tm_hour, timeinfo->tm_min);
                    }
                };
            }
            else if (state) {
                state = false;
                SetPlugState(1, state);   
            }
            
            if (state) {
                onTime += 600;
                if (onTime % 3600 == 0) {
                    fprintf(stdout, "%2d:%2d: Total on time: %d hours\n", timeinfo->tm_hour, timeinfo->tm_min, onTime/3600);
                }
            }
            
            fflush(stdout);
            sleep(600);
         }
    }  
    
    return 0;
}

