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

/*
    Compile:
    g++ -O2 DeHumid.cpp Adafruit_BME280.cpp -lbcm2835 -o dehumid 
    
    Dependencies: flot, BCM280 sensor, libbcm2835
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
#include <fcntl.h>
#include <sys/stat.h>
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
#define LOOP_TIME 30

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

int fdEnr;
struct flock lockEnr;

int EnerginieAquireLock()
{
	char file[] = "/tmp/energine.lock";
	int err = 0;

	/* Open a file descriptor to the file. */
	/* Open a file descriptor to the file. */
  if( access(file, F_OK ) != -1 ) {
    // file exists
    fdEnr = open (file, O_WRONLY);
  } else {
    // file doesn't exist
    // Disable umask to set file permissions    
    mode_t oldmask = umask(0); // needs sys/stat.h
    fdEnr = open (file, O_WRONLY | O_CREAT, 0666);
    umask(oldmask);
  }

	/* Initialize the flock structure. */
	memset (&lockEnr, 0, sizeof(lockEnr));
	lockEnr.l_type = F_WRLCK;

	/* Place a write lock on the file. 
	   Try for upto 5 seconds. */
	for (int i = 0; i < 100; i++) {
		err = fcntl (fdEnr, F_SETLK, &lockEnr);

		if (!err) {
			break;
		}

		usleep(50000);
	}

	return err;
}

int EnerginieReleaseLock()
{
	/* Release the lock. */
	lockEnr.l_type = F_UNLCK;
	int err = fcntl (fdEnr, F_SETLK, &lockEnr);

	close (fdEnr);
	return err;
}

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
 * "for 10 seconds or more until the red light flashes quickly."
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
static int InitPlugs(void) {

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

/* Transmits signal over 1 second period */
static int SetPlugState(int number, bool state)
{
    EnerginieAquireLock();

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
    usleep(50000);

    EnerginieReleaseLock();
    return 0;
}
 
static const char *
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
    bool state = false, lastState = false;
    double maxHumidity = 50;
    double minHumidity = 45;
    struct tm timeStart, timeStop;
    struct Data data;
    int transmitCount = 0;

 	fprintf(stderr, "DeHumid Version: 1.3.0\n\n");
    
    timeStart.tm_hour = 23;
    timeStart.tm_min = 30;
    timeStop.tm_hour = 6;
    timeStop.tm_min = 30;

    /* Print help. */
    if (strstr(argv[1], "-h")) {
        fprintf(stdout, "Dehumidifier controller using Energinie Pi-mote, BME280 sensor and Raspberry Pi.\n");
        fprintf(stdout, "Measurements will be updated every %d minutes.\n\n", LOOP_TIME/60);
        fprintf(stdout, "The Raspberry Pi measures the humidity using a BME280 sensor, and turns on/off the\n");
        fprintf(stdout, "dehumidifier using an Energinie wireless power socket.\n\n");
        fprintf(stdout, "The application can generate a HTML file that can be used to display the results in\n");
        fprintf(stdout, "a graph using the javascript Flot library. A template HTML file 'template_results.html'\n");
        fprintf(stdout, "will be used to create a 'results.html' file. The tag <sensordata//> in the template\n");
        fprintf(stdout, "file will be replaced with measurement data.\n\n");

        fprintf(stdout, "Usage: %s [minHumidity] [maxHumidity] [start time] [stop time]\n", argv[0]);
        fprintf(stdout, "Example: %s 45 47 23:00 06:30\n", argv[0]);
        fprintf(stdout, "This command will turn on the dehumidfier between 11:30pm and 6:30am if the humidity\nis above 47%% until it is below 45%%.\n\n");
        fprintf(stdout, "-t\tThis will test the the dehumidfier power control by turning it on for about\n\t5 seconds, and then turn it off.\n\n");
        fprintf(stdout, "-h\tPrint this help.\n");
        return 0;
    }

    if (strstr(argv[1], "-t")) {
	    if (InitPlugs())
	    {
       	 	perror("Failed to initialise plug GPIO control.\n");
        	return -1;
       	}

        fprintf(stdout, "Testing: \tTurning on ...\n");
        SetPlugState(1, true);
        sleep(4);
        fprintf(stdout, "Testing: \tTurning off ...\n");
        SetPlugState(1, false);
        return 0;
    }
    
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
    fprintf(stdout, "Start time set to %02d:%02d\n", timeStart.tm_hour, timeStart.tm_min);
    fprintf(stdout, "Stop time set to %02d:%02d\n\n", timeStop.tm_hour, timeStop.tm_min);
    
	if (InitPlugs())
	{
   	 	perror("Failed to initialise plug GPIO control.\n");
    	return -1;
   	}

    bool loadedResults = false;
    FILE *fd;

    for (int f = 0; f < 2; f++) {
        if (f == 0) {
   	        fprintf(stdout, "Loading results ... ");
            fd = fopen("dehumid.data", "rb");
                
        }
     	else {
   	        fprintf(stdout, "Loading backup results ... ");
            fd = fopen("dehumid.data.backup", "rb");
   	    }

        if (fd) {
            /* Read struct as 1 record and validate using total. */
            if ((fread(&data, sizeof(struct Data), 1, fd) == 1) && (data.total == MAX_RESULTS)) {
                loadedResults = true;
     	        fprintf(stdout, "success\n");
            }
            else {
     	        fprintf(stderr, "failed - results corrupt or from an older version?\n");
            }
            
            fclose(fd);
        }
        else {
 	        fprintf(stdout, "failed to find file\n");
        }

        if (loadedResults) {
            break;
        }
    }

    if (!loadedResults) {
        /* Initialize results. */
        memset(&data.total, 0, sizeof(struct Data));  
        data.total = MAX_RESULTS;

        for (int d = 0; d < MAX_RESULTS; d++) {
            data.results[d].index = -1;
        }            
    }

    fprintf(stdout, "Looking for HTML template file: 'results_template.html'  ... ");
    fd = fopen("results_template.html", "r");
    bool generateHTML = false;
    if (fd) {
        generateHTML = true;
        fprintf(stdout, "success\n");
        fprintf(stdout, "Using template to generate HTML page 'results.html'.\n");
        fclose(fd);
    }
    else {
 	    fprintf(stdout, "failed to find file\n");
        fprintf(stdout, "Not going to generate HTML results page.\n");
    }
    

    /* Initialise BME280 sensor using address 0x77 or 0x76 on /dev/i2c-1 */
    fprintf(stdout, "Initialising BME280 sensor using address 0x77 ...\n");
    Adafruit_BME280 bme;
    bool bmeSuccess = bme.begin(BME280_ADDRESS2, "/dev/i2c-1");

    if (bmeSuccess == false) {
        fprintf(stdout, "\nInitialising BME280 sensor using address 0x76 ...\n");
        bmeSuccess = bme.begin(BME280_ADDRESS, "/dev/i2c-1");
    }

    if (!bmeSuccess) {
        fprintf(stderr, "ERROR: Failed to initialise BME280 on address 0x76 or 0x77!\n");
        return 1;
    }
    else {
        fprintf(stdout, "Successfully initialised BME280 sensor.\n\n");
    }

    /* Start with plugs off. */    
    SetPlugState(1, false);  
    /* Retransmit signal later to ensure that plug group is off. */
    transmitCount = 3; 

    while (true) {
        time_t rawtime;
        struct tm * timeinfo;

        /* Get the index of the oldest result. */
        int i = data.count % MAX_RESULTS;
        
        ::time(&rawtime);
        timeinfo = localtime (&rawtime);
        fprintf(stdout, "%02d:%02d: ", timeinfo->tm_hour, timeinfo->tm_min);

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
                    fprintf(stdout, "%02d:%02d: Turned ON\n", timeinfo->tm_hour, timeinfo->tm_min);
                }
            }
            else if (data.results[i].humidity < minHumidity) {
                if (state != false) {
                    state = false;
                    fprintf(stdout, "%02d:%02d: Turned OFF\n", timeinfo->tm_hour, timeinfo->tm_min);
                }
            };
        }
        else if (state) {
            state = false;
            fprintf(stdout, "%02d:%02d: Turned OFF\n", timeinfo->tm_hour, timeinfo->tm_min);
        }
        
        fflush(stdout);

        if (state) {
            data.onTime += LOOP_TIME;
            if (data.onTime % 1800 == 0) {
                fprintf(stdout, "%02d:%02d: Total on time: %.1f hours\n", timeinfo->tm_hour, timeinfo->tm_min, data.onTime/3600.0);
            }
        }
        
        if ((lastState) && (!state)) {
            /* Ensure that plug on times are correct by including the last period. */
            data.results[i].state = lastState;
        }
        else {
            /* Update plug state field. */
            data.results[i].state = state;
        }
        
        /* Re-transmit counter. */
        transmitCount = 3;
        
        lastState = state;

        data.count++;
        
        FILE *fileRead, *fileWrite;
        char line[4096];

        if (generateHTML) {
            fileRead = fopen("results_template.html", "r");
            fileWrite = fopen("results.html", "w+");
            
            if (!fileRead) {
                fprintf(stderr, "ERROR: Failed to open 'results_template.html' for reading!\n");
            }
            else if (!fileWrite) {
                fprintf(stderr, "ERROR: Failed to open 'results.html' for writing!\n");
            }
            else {
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

                        fprintf(fileWrite, "var dehumidifier = [");
                        for (int d = i+1; d < MAX_RESULTS; d++) {
                            if (data.results[d].index >= 0) {
                                if (data.results[d].state) {
                                    fprintf(fileWrite, "[%ld000,1],", data.results[d].timestamp);
                                }
                                else {
                                    fprintf(fileWrite, "[%ld000,0],", data.results[d].timestamp);
                                }
                            }
                        }            
                        for (int d = 0; d < i; d++) {
                            if (data.results[d].index >= 0) {
                                if (data.results[d].state) {
                                    fprintf(fileWrite, "[%ld000,1],", data.results[d].timestamp);
                                }
                                else {
                                    fprintf(fileWrite, "[%ld000,0],", data.results[d].timestamp);
                                }
                            }
                        }            
                        fprintf(fileWrite, "];\n");
                    }
                    else if (strstr(line, "<currentreading/>")) {
                        fprintf(fileWrite, "var currentreading = [");
                        fprintf(fileWrite, "[%ld,%f,%f,%f]];\n", data.results[i].timestamp, data.results[i].temprature, data.results[i].pressure, data.results[i].humidity);
                    }
                    else {
                        fprintf(fileWrite, "%s", line); 
                    }
                }
            }
            if (fileRead) {
                fclose(fileRead);
            }
            
            if (fileWrite) {
                fclose(fileWrite);
            }
        }
        
        /* Save Results. Alternate between .data and .backup*/
        if (i % 2 == 0) {
            fd = fopen("dehumid.data", "wb");
        }
        else {
            fd = fopen("dehumid.data.backup", "wb");
        }

        /* Re-initialise sensor every 12*looptime = 60 minutes. */
        if (i % 12 == 0) {
            fprintf(stdout, "\nRe-initialising BME280 sensor ...\n");
            bool bmeSuccess = bme.begin(BME280_ADDRESS2, "/dev/i2c-1");

            if (bmeSuccess == false) {
                bmeSuccess = bme.begin(BME280_ADDRESS, "/dev/i2c-1");
            }

            if (!bmeSuccess) {
               	fprintf(stderr, "ERROR: Failed to re-initialise BME280 on address 0x76 or 0x77!\n");
                return 1;
            }
            else {
                fprintf(stdout, "Successfully re-initialised BME280 sensor.\n\n");
            }
        }

        fwrite(&data, sizeof(struct Data), 1 , fd);            
        fclose(fd);           

        if (transmitCount > 0) {
            /* Re-transmit signal to ensure that plug group is set correctly. */
            for (int r = 0; r < transmitCount; r++) {  
                SetPlugState(1, state); 
                sleep(LOOP_TIME/transmitCount);
            }
            
            transmitCount = 0;
        }
        else {
            sleep(LOOP_TIME);
        }
    }
    
    return 0;
}

