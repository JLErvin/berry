#!/bin/bash

make || exit 1
sudo make install
pkill sxhkd

Xephyr -screen 640x480 -screen 640x480 +xinerama :80 &
sleep 1

export DISPLAY=:80
sxhkd -c examples/sxhkdrc &

while sleep 1; do berry -c examples/autostart -d; done
