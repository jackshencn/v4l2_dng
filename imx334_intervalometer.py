#!/usr/bin/python

import os
import sys
import time
from multiprocessing import Process

channel = int(sys.argv[1])
total_capture = int(sys.argv[2])
interval = int(sys.argv[3])

xhs_time = 7.41
max_exposure = int((interval - 1.2) * 1000000 / xhs_time)

TARGET_EXPOSURE = 4000
cmd = "media-ctl -d /dev/media"
cmd += str(channel) + " --set-v4l2 '1:2[fmt:SRGGB12_1X12/3864x2180]'"
os.popen(cmd)

video_ch = 0
if channel == 1:
    video_ch = 5

cap_cmd = "v4l2-ctl -d /dev/video" + str(video_ch)
cap_cmd += " --set-fmt-video=width=3864,height=2180,pixelformat=BG12"
cap_cmd += " --stream-mmap=1 --stream-to=/tmp/IMX334.raw --stream-count=1"
pro_cmd = "./raw2dng 3864 2180 12 /tmp/IMX334.raw /media/pi/1A07-EE52/"
v4l2_set_vblank = "v4l2-ctl -d /dev/video0 --set-ctrl=vertical_blanking="
v4l2_set_exposure = "v4l2-ctl -d /dev/video0 --set-ctrl=exposure="

def capture(index):
    exposure_rows = os.popen("v4l2-ctl -d /dev/video0 --get-ctrl=exposure").readlines()[0]
    exposure_rows = int(exposure_rows.split(' ')[1])
    print(str(index) + ": Current exposure: " + str(exposure_rows))
    t = time.localtime() # Get time at the start of exposure
    os.popen(cap_cmd)
    cur_time = time.strftime("%Y%m%d_%H%M%S", t)
    max_value = os.popen(pro_cmd + cur_time + ".DNG").readlines()
    max_value = float(max_value[0].rstrip())
    print(str(index) + ": Saved " + cur_time + ".DNG")
    print(str(index) + ": Highlight @" + str(max_value))
    if max_value > 4090:
        exposure_rows = int(exposure_rows * 0.5)
    else:
        exposure_rows = int(exposure_rows * (TARGET_EXPOSURE / max_value))
    if exposure_rows > max_exposure:
        exposure_rows = max_exposure
    exp_time = exposure_rows * xhs_time / 1000000
    print(str(index) + ": Set exposure to " + str(exposure_rows) + " rows or " + str(exp_time) + "s")
    os.popen(v4l2_set_vblank + str(exposure_rows + 2250))
    os.popen(v4l2_set_exposure + str(exposure_rows))
    print("")

for index in range(total_capture):
    time.sleep(interval)
    p = Process(target = capture, args=(index,))
    p.start()

