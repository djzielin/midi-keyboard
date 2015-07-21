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

while True:
	try:
		GPIO.setmode(GPIO.BCM)
		GPIO.setup(switchpin, GPIO.IN, pull_up_down=GPIO.PUD_UP)

		GPIO.setup(lightpin2, GPIO.OUT)
        	GPIO.output(lightpin2,GPIO.HIGH)

		GPIO.setup(lightpin1, GPIO.OUT)
		GPIO.output(lightpin1,GPIO.LOW)

		GPIO.wait_for_edge(switchpin, GPIO.FALLING);

		GPIO.setup(lightpin1, GPIO.OUT)
		GPIO.output(lightpin1,GPIO.HIGH);

		print "\nFalling edge detected."
		Popen(['nohup', '/home/pi/midi-keyboard/launch_keyboard.sh'])
		print("sleeping for 3 seconds");
                time.sleep(3);
                print("turning the light back on"); 
        
		if(GPIO.input(switchpin)==0): #if the user is is still holding the button, we are taking the whole system down
			print("shutting it down")
			#subprocess.call(['killall keyboard'], shell=True)
       	    		#time.sleep(1);
		
			GPIO.setup(lightpin1, GPIO.OUT)
        		GPIO.output(lightpin1,GPIO.LOW);

                        #GPIO.setup(26,GPIO.OUT)
                        #GPIO.output(26,GPIO.HIGH)
    		

			subprocess.call(['shutdown -h now "System halted by GPIO action"'], shell=True)
			while True:
                                print("just sleeping...")
				time.sleep(1);
		
		GPIO.cleanup()   # bug in edge detect causes retriggers
                
	except KeyboardInterrupt:
		GPIO.cleanup()       # clean up GPIO on CTRL+C exit
GPIO.cleanup()           # clean up GPIO on normal exit
