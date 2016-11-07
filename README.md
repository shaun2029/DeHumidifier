#DeHumidifier Control
Dehumidifier controller using Energinie Pi-mote, BME280 sensor and Raspberry Pi. It saves on electricity bills by limiting the use of the dehumidifier/s to only when needed, and optionally during times when electricity is cheaper.

The Raspberry Pi measures the humidity using a [BME280](https://www.sparkfun.com/products/13676) sensor, and turns on/off the dehumidifie/s using a [Energinie](https://energenie4u.co.uk/index.phpcatalogue/product/ENER002-2PI) wireless power socket.

####DeHumidifier Control generates a HTML file that can be used to display the results in a graph.
![](https://github.com/shaun2029/DeHumidifier/blob/master/docs/Graph.png?raw=true)

##Requires: 
Raspberry Pi (B+/2/3/Zero), Energinie Pi-mote, BCM280 sensor breakout board.

<img src="https://energenie4u.co.uk/res/images/products/large/ENER002-2PI.jpg" alt="Energinie Power Sockets and Wireless Controller" width="400"/> <img src="https://cdn.sparkfun.com//assets/parts/1/1/1/2/6/13676-01.jpg" alt="BME280" width="200"/>

##Instructions:
Connect the BCM280 breakout board to Rapsberry Pi I2C pins:
<table>
    <tr>
        <th> PI GPIO Pin </th><th> BME280 Pin</th>
    </tr>
    <tr>
        <th> 1 (3v3)  </td><td> VCC </td>
    </tr>
    <tr>
        <th> 3 (BCM2) </td><td> SDA </td>
    </tr>
    <tr>
        <th> 5 (BCM3) </td><td> SCL </td>
    </tr>
    <tr>
        <th> 6 (GND)  </td><td> GND </td>
    </tr>
</table>
Enable i2c support on the Pi using raspi-config (Advanced menu option).

The application dehumid update the measurements every five minutes. It measures the humidity using a BME280 sensor, and turns on/off the dehumidifier using an Energinie Pi wireless remote and power socket.

The application can generate a HTML file that can be used to display the results in a graph. This uses the javascript Flot library. A template HTML file 'template_results.html' will be used to create a 'results.html' file. The tag <sensordata/> in the template file will be replaced with measurement data.

##Compile:
g++ -O2 DeHumid.cpp Adafruit_BME280.cpp -lbcm2835 -o dehumid 

##Compile dependencies: 
bcm2835

Note: Included in libs folder.

####bcm2835:
http://www.airspayce.com/mikem/bcm2835
C library for Broadcom BCM 2835 as used in Raspberry Pi
This is a C library for Raspberry Pi (RPi). It provides access to GPIO and other IO functions on the Broadcom BCM 2835 chip, allowing access to the GPIO pins on the 26 pin IDE plug on the RPi board so you can control and interface with various external devices.

####Flot
https://github.com/flot/flot
Flot is a Javascript plotting library for jQuery.
Read more at the website: http://www.flotcharts.org/


