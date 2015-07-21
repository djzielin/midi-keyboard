sudo killall drums
cd /home/pi/midi-drums
sudo chrt 90 ./drums -f ./configs/$1
