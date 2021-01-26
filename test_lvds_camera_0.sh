#!/bin/sh

# this is FB based test
killall weston
killall capture

while true; do
capture -d /dev/video0 -F -f rgb32 -L 0 -T 0 -W 1280 -H 800 -c 1000 -z
done
