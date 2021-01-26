#!/bin/sh

# this is FB based test
killall capture

while true; do
capture -D 12 -F -f rgb32 -L 400 -T 130 -W 320 -H 540 -c 1000 -z
done
