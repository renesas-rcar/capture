#!/bin/sh

# this is FB based test
killall weston
killall capture

# count = 30fps*3sec
while true; do
capture -d /dev/video4 -F -f rgb32 -L 0 -T 0 -W 1280 -H 800 -c 100 -z
capture -d /dev/video5 -F -f rgb32 -L 0 -T 0 -W 1280 -H 800 -c 100 -z
capture -d /dev/video6 -F -f rgb32 -L 0 -T 0 -W 1280 -H 800 -c 100 -z
capture -d /dev/video7 -F -f rgb32 -L 0 -T 0 -W 1280 -H 800 -c 100 -z
done
