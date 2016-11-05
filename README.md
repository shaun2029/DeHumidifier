#DeHumidifier
Dehumidifier controller using Energinie Pi-mote, BME280 sensor and Raspberry Pi.

#Requires: 
Raspberry Pi (B+/2/3/Zero), BCM280 sensor.

#Instructions:
Connect the BCM280 breakout board to Rapsberry Pi I2C pins:
<table>
    <tr>
        <th> PI Pin </th><th> BME280 Pin</th>
    </tr>
    <tr>
        <th> 1 (3v3) </td><td> VCC </td>
    </tr>
    <tr>
        <th> 3 (BCM2) </td><td> SDA </td>
    </tr>
    <tr>
        <th> 5 (BCM3) </td><td> SCL </td>
    </tr>
    <tr>
        <th> 6 (GND) </td><td> GND </td>
    </tr>
</table>
Enable i2c support on the Pi using raspi-config (Advanced menu option).

#Compile:
g++ -O2 DeHumid.cpp Adafruit_BME280.cpp -lbcm2835 -o dehumid 

#Compile dependencies: 
Raspberry Pi (B+/2/3/Zero), BCM280 sensor, libbcm2835, flot.

# bcm2835:
http://www.airspayce.com/mikem/bcm2835
C library for Broadcom BCM 2835 as used in Raspberry Pi
This is a C library for Raspberry Pi (RPi). It provides access to GPIO and other IO functions on the Broadcom BCM 2835 chip, allowing access to the GPIO pins on the 26 pin IDE plug on the RPi board so you can control and interface with various external devices.

#Flot
https://github.com/flot/flot
Flot is a Javascript plotting library for jQuery.
Read more at the website: http://www.flotcharts.org/

