#!/bin/sh

sleep 5
FRAME_PERIOD="${1-2.5}"

media-ctl -d /dev/media1 --set-v4l2 '8:0[fmt:SRGGB12_1X12/3864x2180]'
media-ctl -d /dev/media1 --set-v4l2 '1:0[fmt:SRGGB12_1X12/3864x2180 crop:(0,0)/3864x2180]'
media-ctl -d /dev/media1 --set-v4l2 '1:2[fmt:SRGGB12_1X12/3864x2180 crop:(0,0)/3864x2180]'

DIR_NAME=$(date '+%Y%m%d_%H%M')
cd /media/pi/32G_SD/ && \
    mkdir $DIR_NAME && cd $DIR_NAME && \
    stdbuf -o0 /home/pi/v4l2_dng/intervalometer 3864 2180 $FRAME_PERIOD 500 > exposure.log

