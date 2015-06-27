#! /bin/sh


# Carry out specific functions when asked to by the system
case "$1" in
  start)
    echo "Starting script blah "
    /home/pi/midi-keyboard/pi_gpio.py &
    ;;
  stop)
    echo "Stopping script blah"
    echo "not sure how to stop specific instance of python"
    ;;
  *)
    echo "Usage: /etc/init.d/reset_button.sh {start|stop}"
    exit 1
    ;;
esac

exit 0
