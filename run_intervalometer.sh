#!/bin/sh

sleep 10
FRAME_PERIOD="${1-2.5}"
VBLK=$(echo $FRAME_PERIOD*2210*50-2180 | bc)

v4l2-ctl -d /dev/video0 --set-ctrl=vertical_blanking=$VBLK
media-ctl -d /dev/media0 --set-v4l2 '8:0[fmt:SRGGB12_1X12/3864x2180]'
media-ctl -d /dev/media0 --set-v4l2 '1:0[fmt:SRGGB12_1X12/3864x2180 crop:(0,0)/3864x2180]'
media-ctl -d /dev/media0 --set-v4l2 '1:2[fmt:SRGGB12_1X12/3864x2180 crop:(0,0)/3864x2180]'
v4l2-ctl -d /dev/video0 --set-selection=target=crop,width=3864,height=2180 \
    --set-fmt-video=width=3864,height=2180,pixelformat=BG12 

DIR_NAME=$(date '+%Y%m%d_%H%M')
cd /media/pi/32G_SD/ && \
    mkdir $DIR_NAME && cd $DIR_NAME && \
    stdbuf -o0 /home/pi/v4l2_dng/intervalometer 3864 2180 12 500 > exposure.log

