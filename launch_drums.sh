sudo killall drums
sudo killall keyboard
cd /home/pi/midi-drums
sudo chrt 90 ./drums -f $1
