#!/usr/bin/env python3
# coding=utf-8

import psutil, json
import time
import serial
import sys
import argparse
import pystray

from PIL import Image, ImageDraw
from pystray import Menu, MenuItem

# Get disk usage information
# Search all disk and get usages
# Disk/partition are filtered to remove unwanted
def disk_usage(json_key):
    hdd = psutil.disk_partitions()
    data = []
    # iterate all mounted parts/disk
    for each in hdd:
        path = each.mountpoint
        fstype = each.fstype
        # filter unwanted devices (LINUX)
        if sys.platform.startswith("linux"):
            # LINUX
            # keep only ext4 (may need to change according to your system)
            if fstype != "ext4": 
                continue
            # Hide /var/* (snap)
            if path.startswith("/var/"):
                continue

            # Displayed label
            label = path
            if path.startswith("/mnt/"):
                label = path.replace("/mnt/", "")
        else:
            # WINDOWS
            # keep only NTFS (may need to change according to your system)
            if fstype != "NTFS":
                continue
            label = path

        # Get uesage information
        drive = psutil.disk_usage(path)
        total = drive.total
        total = total / 1000000000
        used = drive.used
        used = used / 1000000000
        percent = drive.percent
        drives = {
            "path": label,
            "total": float("{0: .2f}".format(total)),
            "used": float("{0: .2f}".format(used))
        }
        data.append(drives)
    json_key['DISKS'] = data

# Get CPU cores usages
def cpu_usage(json_key, delay):
    cpu_usage = psutil.cpu_percent(interval=delay, percpu=True)
    json_key['CPU'] = cpu_usage

# Get CPU temp
def cpu_temp(json_key):
    if sys.platform.startswith("linux"):
        temps = psutil.sensors_temperatures()
        json_key['TEMP'] = temps['amdgpu'][0][1]
    else:
        json_key['TEMP'] = 0 # Not supported by psutil

# Get Mem usage in %
def percent_mem(json_key):
    mem_percent = psutil.virtual_memory().percent
    json_key['RAM'] = mem_percent

def create_image(width, height, color1, color2):
    # Generate an image and draw a pattern
    image = Image.new('RGB', (width, height), color1)
    dc = ImageDraw.Draw(image)
    dc.rectangle((width // 2, 0, width, height // 2),  fill=color2)
    dc.rectangle((0, height // 2, width // 2, height), fill=color2)
    return image

# Serial port
ser = None
args = None

contflag = True
delay = 0

def work_loop(icon ):
    global args
    global ser
    global contflag
    global delay
    
    delay = args.delay
    icon.visible = True
    # Main loop (connect/reconnect)
    while contflag:
        try:
            ser = serial.Serial(args.port, baudrate=19200)  # open serial port
            print('Opening:' + ser.name + ' at ' + str(19200))

            # Data loop Until disconnected (exception) grab and send data
            while contflag:
                json_key={}
                cpu_usage(json_key, delay)
                cpu_temp(json_key)
                percent_mem(json_key)
                disk_usage(json_key)
                #print(json.dumps(json_key), end='\r')
                data = json.dumps(json_key) 
                ser.write(data.encode('ascii'))

        except serial.SerialException as e:
            if ser and ser.is_open:
                ser.close()
            print('Disconnected: ' + str(e))

        # Wait before reconnect except if exiting
        if contflag:
            time.sleep(5)
        
    # Close serial port
    if ser and ser.is_open:
        ser.close()

# Menu of systray
# Exit callback
def exit_action(icon):
    global contflag
    
    contflag = False
    icon.visible = False  # Need it to stop the main while loop. I think any global var also will do the thing
    icon.stop()  # Stop icon thread (?)

# Change delay
def set_delay( d ):
    global delay
    delay = d

def get_delay( d ):
    global delay
    return delay == d

# Main
def main():
    global args
    global ser
    
    # parse args
    argParser = argparse.ArgumentParser()
    argParser.add_argument("-d", "--delay", help="Period for grabbing data (s)", type=float, default=0.5)
    if sys.platform.startswith("linux"):
        argParser.add_argument("-p", "--port", help="Serial port of pico", default="/dev/ttyACM0")
    else:
        argParser.add_argument("-p", "--port", help="Serial port of pico", default="COM3")

    args = argParser.parse_args()

    # In order for the icon to be displayed, you must provide an icon
    icon = pystray.Icon('PiCoMonitor', icon=create_image(64, 64, 'blue', 'white'))
    icon.menu = Menu(
                        MenuItem('0.25 s', lambda i : set_delay(0.25), checked=lambda i :get_delay(0.25)),
                        MenuItem('0.50 s', lambda i : set_delay(0.5), checked=lambda i :get_delay(0.5)),
                        MenuItem('1.00 s', lambda i : set_delay(1.0), checked=lambda i :get_delay(1.0)),
                        Menu.SEPARATOR,
                        MenuItem('Exit', lambda : exit_action(icon)))

    icon.title = 'PiCoMonitor informations grab tool'
    # To finally show you icon, call run
    icon.run(work_loop)
    
if __name__ == "__main__":
    main()
