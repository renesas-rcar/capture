#!/bin/sh

# this is FB based test
killall weston
killall capture

while true; do
capture -D 4 -F -f rgb32 -L 160 -T 130 -W 960 -H 540 -c 1000 -z
done
