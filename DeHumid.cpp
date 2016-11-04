/* DeHumid.cpp
  
    Dehumidifier control using BME280 and the 
    Energinie ENER314 RASPBERRY P I RF- TRANSMITTER BOARD

    Author: Shaun Simpson
    Copyright (C) 2016 Shaun Simpson
   
    This file is part of DeHumidifier.

    DeHumidifier is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    DeHumidifier is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeHumidifier.  If not, see <http://www.gnu.org/licenses/>.   
*/


#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include "bcm2835.h"
#include "Adafruit_BME280.h"

#define SEALEVELPRESSURE_HPA (1013.25)

#define GPIO17 RPI_V2_GPIO_P1_11
#define GPIO22 RPI_V2_GPIO_P1_15
#define GPIO23 RPI_V2_GPIO_P1_16
#define GPIO27 RPI_V2_GPIO_P1_13
#define GPIO24 RPI_V2_GPIO_P1_18
#define GPIO25 RPI_V2_GPIO_P1_22

/* Loop time in seconds. */
#define LOOP_TIME 300

/* Store two weeks results. */
#define MAX_RESULTS 4032

struct SensorResult {
    int index;
    struct tm time;
    time_t timestamp;
    bool state;
    double temprature;
    double pressure;
    double humidity;
} __attribute__((__packed__));

struct Data {
    int32_t total;
    uint64_t count;
    uint32_t onTime;
    struct SensorResult results[MAX_RESULTS];
} __attribute__((__packed__));

struct Data data;

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
	    
	    sleep(1); 
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

 	fprintf(stderr, "DeHumid Version: 1.1.0\n");
    
    timeStart.tm_hour = 23;
    timeStart.tm_min = 30;
    timeStop.tm_hour = 6;
    timeStop.tm_min = 30;


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

    FILE *fd = fopen("dehumid.data", "rb");
    int bytes = 0;

    if (fd) {
 	    fprintf(stdout, "Loading results ... ");
        bytes = fread(&data, sizeof(struct Data), 1, fd);
        
        if ((bytes == 1) && (data.total == MAX_RESULTS)) {
 	        fprintf(stdout, "success\n");
        }
        else {
 	        fprintf(stdout, "failed bytes read %d, total %d\n", bytes, data.total);
        }
        
        fclose(fd);
    }

    if ((bytes != 1) || (data.total != MAX_RESULTS)) {
        /* Initialize results. */
        memset(&data.total, 0, sizeof(struct Data));  
        data.total = MAX_RESULTS;

        for (int d = 0; d < MAX_RESULTS; d++) {
            data.results[d].index = -1;
        }            
    }

    if (bme.begin(BME280_ADDRESS, "/dev/i2c-1")) {
        /* Give sensor some time. */
        usleep(100000);
        SetPlugState(1, state);   
    
        while (true) {
            time_t rawtime;
            struct tm * timeinfo;

            /* Get the index of the oldest result. */
            int i = data.count % MAX_RESULTS;
            
            time (&rawtime);
            timeinfo = localtime (&rawtime);
            fprintf(stdout, "%2d:%2d: ", timeinfo->tm_hour, timeinfo->tm_min);

            /* Record result. */           
            data.results[i].index = data.count;
            data.results[i].time = *timeinfo;
            data.results[i].timestamp = rawtime;
            data.results[i].temprature = bme.readTemperature();
            data.results[i].pressure = bme.readPressure() / 100.0F;
            data.results[i].humidity = bme.readHumidity();          
            
            fprintf(stdout, "Temperature = %.2f *C\t", data.results[i].temprature);
            fprintf(stdout, "Pressure = %.2f hPa\t", data.results[i].pressure);
            fprintf(stdout, "Humidity = %.2f\n", data.results[i].humidity);
            
            int tMin = timeinfo->tm_hour * 60 + timeinfo->tm_min;
           
            /* Control if time between Start and Stop */
            if ((tMin >= timeStart.tm_hour*60+timeStart.tm_min) || (tMin < timeStop.tm_hour*60+timeStop.tm_min)) {
                if (data.results[i].humidity > maxHumidity) {
                    if (state != true) {
                        state = true;
                        SetPlugState(1, state);   
                        fprintf(stdout, "%2d:%2d: Turned ON\n", timeinfo->tm_hour, timeinfo->tm_min);
                    }
                }
                else if (data.results[i].humidity < minHumidity) {
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
                fprintf(stdout, "%2d:%2d: Turned OFF\n", timeinfo->tm_hour, timeinfo->tm_min);
            }
            
            fflush(stdout);
            sleep(LOOP_TIME);

            if (state) {
                data.onTime += LOOP_TIME;
                if (data.onTime % 1800 == 0) {
                    fprintf(stdout, "%2d:%2d: Total on time: %.1f hours\n", timeinfo->tm_hour, timeinfo->tm_min, data.onTime/3600.0);
                }
            }
            
            data.results[i].state = state;
            data.count++;
            
            FILE *fileRead, *fileWrite;
            char line[4096];

            fileRead = fopen("results_template.html", "r");
            
            if (!fileRead) {
                fprintf(stderr, "ERROR: Failed to open 'results_template.html' for reading!\n");
                return 1;
            }
            else {
                fileWrite = fopen("results.html", "w+");
                
                while (fgets(line, sizeof(line), fileRead)) {
                    /* note that fgets does not strip the terminating \n, checking its
                     * presence would allow to handle lines longer that sizeof(line) */
                    
                    if (strstr(line, "<sensordata/>")) {
                        fprintf(fileWrite, "var temprature = [");
                        for (int d = i+1; d < MAX_RESULTS; d++) {
                            if (data.results[d].index >= 0) {
                                fprintf(fileWrite, "[%ld000,%f],", data.results[d].timestamp, data.results[d].temprature);
                            }
                        }            
                        for (int d = 0; d < i; d++) {
                            if (data.results[d].index >= 0) {
                                fprintf(fileWrite, "[%ld000,%f],", data.results[d].timestamp, data.results[d].temprature);
                            }
                        }            
                        fprintf(fileWrite, "];\n");

                        fprintf(fileWrite, "var pressure = [");
                        for (int d = i+1; d < MAX_RESULTS; d++) {
                            if (data.results[d].index >= 0) {
                                fprintf(fileWrite, "[%ld000,%f],", data.results[d].timestamp, data.results[d].pressure);
                            }
                        }            
                        for (int d = 0; d < i; d++) {
                            if (data.results[d].index >= 0) {
                                fprintf(fileWrite, "[%ld000,%f],", data.results[d].timestamp, data.results[d].pressure);
                            }
                        }            
                        fprintf(fileWrite, "];\n");
                        
                        fprintf(fileWrite, "var humidity = [");
                        for (int d = i+1; d < MAX_RESULTS; d++) {
                            if (data.results[d].index >= 0) {
                                fprintf(fileWrite, "[%ld000,%f],", data.results[d].timestamp, data.results[d].humidity);
                            }
                        }            
                        for (int d = 0; d < i; d++) {
                            if (data.results[d].index >= 0) {
                                fprintf(fileWrite, "[%ld000,%f],", data.results[d].timestamp, data.results[d].humidity);
                            }
                        }            
                        fprintf(fileWrite, "];\n");
                    }
                    else {
                        fprintf(fileWrite, "%s", line); 
                    }
                }
                
                fclose(fileRead);
            }
            fclose(fileWrite);
            
            /* Save Results. */
            fd = fopen("dehumid.data", "wb");
            fwrite(&data, sizeof(struct Data), 1 , fd);            
            fclose(fd);           
         }
    }  
    
    return 0;
}

