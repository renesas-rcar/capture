#!/bin/sh

# this is FB based test
killall weston
killall capture

while true; do
capture -D 2 -F -f rgb32 -L 160 -T 0 -W 960 -H 800 -c 1000 -z
done
