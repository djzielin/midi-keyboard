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

drumset=0;

while True:
	try:
		GPIO.setmode(GPIO.BCM)
		GPIO.setup(switchpin, GPIO.IN, pull_up_down=GPIO.PUD_UP)

		GPIO.setup(lightpin2, GPIO.OUT)
        	GPIO.output(lightpin2,GPIO.HIGH)

		GPIO.setup(lightpin1, GPIO.OUT)
		GPIO.output(lightpin1,GPIO.LOW)

                print("waiting for button press...");
		GPIO.wait_for_edge(switchpin, GPIO.FALLING);

		GPIO.setup(lightpin1, GPIO.OUT)
		GPIO.output(lightpin1,GPIO.HIGH);

		print("  Button Press detected!")

		try:
                	subprocess.check_call(['/home/pi/midi-keyboard/get_instrument_connected'], shell=True)
		except subprocess.CalledProcessError as myexcept:
              		instrument=myexcept.returncode;

                        if(instrument==1):
				print("Launching guitar!");
  	 			Popen(['nohup', '/home/pi/midi-keyboard/launch_guitar.sh'])
                        if(instrument==2):
				print("Launching keyboard!\n");
  	 			Popen(['nohup', '/home/pi/midi-keyboard/launch_keyboard.sh'])
                        if(instrument==3):
				print("Launching drums!\n");
                		if(drumset==0):
			 		Popen(['nohup', '/home/pi/midi-keyboard/launch_drums.sh','new_linn.cfg'])
                		if(drumset==1):
			 		Popen(['nohup', '/home/pi/midi-keyboard/launch_drums.sh','new_808.cfg'])
                		if(drumset==2):
			 		Popen(['nohup', '/home/pi/midi-keyboard/launch_drums.sh','new_amen.cfg'])
                		if(drumset==3):
			 		Popen(['nohup', '/home/pi/midi-keyboard/launch_drums.sh','vibes.cfg'])
				drumset+=1;
                                if(drumset>3):
					drumset=0;
				print("next drumset will be: %d" % drumset);
		#print("sleeping for 3 seconds");
                time.sleep(3);
        
		if(GPIO.input(switchpin)==0): #if the user is is still holding the button, we are taking the whole system down
			print("shutting it down")
		
			GPIO.setup(lightpin1, GPIO.OUT)
        		GPIO.output(lightpin1,GPIO.LOW);

			subprocess.call(['shutdown -h now "System halted by GPIO action"'], shell=True)
			while True:
                                print("just sleeping...")
				time.sleep(1);
		
		GPIO.cleanup()   # bug in edge detect causes retriggers
                
	except KeyboardInterrupt:
		GPIO.cleanup()       # clean up GPIO on CTRL+C exit
GPIO.cleanup()           # clean up GPIO on normal exit
