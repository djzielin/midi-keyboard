#!/usr/bin/env python2.7
# script by Alex Eames http://RasPi.tv/
# http://raspi.tv/2013/how-to-use-interrupts-with-python-on-the-raspberry-pi-and-rpi-gpio
# heavily modified by DJZ 5/31/2015

import subprocess
from subprocess import Popen
import time

import RPi.GPIO as GPIO
GPIO.setmode(GPIO.BCM)
pin = 6

print "Waiting for falling edge on port 6"
# now the program will do nothing until the signal on port 23
# starts to fall towards zero. This is why we used the pullup
# to keep the signal high and prevent a false interrupt

while True:
	try:
		GPIO.setmode(GPIO.BCM)
		GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)

		GPIO.setup(16, GPIO.OUT)
        	GPIO.output(16,GPIO.HIGH);

		GPIO.wait_for_edge(pin, GPIO.FALLING)
		GPIO.output(16,GPIO.LOW);

		print "\nFalling edge detected."
		Popen(['nohup', '/home/pi/audio/keyboard/launch_keyboard.sh'])
		print("sleeping for 3 seconds");
                time.sleep(3);
                print("turning the light back on"); 
        
		if(GPIO.input(pin)==0):
			print("shutting it down")
			subprocess.call(['killall keyboard'], shell=True)
       	    		time.sleep(1);
		
			GPIO.setup(16, GPIO.OUT)
        		GPIO.output(16,GPIO.HIGH);

                        GPIO.setup(26,GPIO.OUT)
                        GPIO.output(26,GPIO.HIGH)
    		

			subprocess.call(['shutdown -h now "System halted by GPIO action"'], shell=True)
			while True:
                                print("just sleeping...")
				time.sleep(1);
		
		GPIO.cleanup()   # bug in edge detect causes retriggers
                
	except KeyboardInterrupt:
		GPIO.cleanup()       # clean up GPIO on CTRL+C exit
GPIO.cleanup()           # clean up GPIO on normal exit
