#!/bin/bash

cd mjpg-streamer-code-182/mjpg-streamer
mjpg_streamer -i "/usr/lib/input_uvc.so -d /dev/video0 -q 75 -r 320x240" -o "/usr/lib/output_http.so -p 8090 -w /var/www/mjpg_streamer" &
