#!/usr/bin/env python2.7
# script by Alex Eames http://RasPi.tv/
# http://raspi.tv/2013/how-to-use-interrupts-with-python-on-the-raspberry-pi-and-rpi-gpio
# heavily modified by DJZ 5/31/2015

import subprocess
from subprocess import Popen
import time

import RPi.GPIO as GPIO
GPIO.setmode(GPIO.BCM)

switchpin=17
lightpin1=5
lightpin2=26
lightpin3=13;


GPIO.setmode(GPIO.BCM);

GPIO.setup(switchpin, GPIO.IN, pull_up_down=GPIO.PUD_UP);

GPIO.setup(lightpin1, GPIO.OUT);
GPIO.setup(lightpin2, GPIO.OUT);
GPIO.setup(lightpin3, GPIO.OUT);

while True:
	try: 
           	print("turning  on led's. button is: %d" % GPIO.input(switchpin));
        	GPIO.output(lightpin1,GPIO.HIGH);
		GPIO.output(lightpin2,GPIO.HIGH);
		GPIO.output(lightpin3,GPIO.HIGH);

                time.sleep(1);

           	print("turning off led's. button is: %d" % GPIO.input(switchpin));
        	GPIO.output(lightpin1,GPIO.LOW);
		GPIO.output(lightpin2,GPIO.LOW);
		GPIO.output(lightpin3,GPIO.LOW);
 
                time.sleep(1);



	except KeyboardInterrupt:
		GPIO.cleanup()       # clean up GPIO on CTRL+C exit
GPIO.cleanup()           # clean up GPIO on normal exit
