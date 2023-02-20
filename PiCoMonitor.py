#!/usr/bin/env python3
# coding=utf-8

import psutil, json
import time
import serial
import sys
import argparse

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
        json_key['TEMP'] = 0.0 # Not supported by psutil

# Get Mem usage in %
def percent_mem(json_key):
    mem_percent = psutil.virtual_memory().percent
    json_key['RAM'] = mem_percent

# Main
def main():

    # parse args
    argParser = argparse.ArgumentParser()
    argParser.add_argument("-d", "--delay", help="Period for grabbing data (s)", type=float, default=0.5)
    if sys.platform.startswith("linux"):
        argParser.add_argument("-p", "--port", help="Serial port of pico", default="/dev/ttyACM0")
    else:
        argParser.add_argument("-p", "--port", help="Serial port of pico", default="COM3")

    args = argParser.parse_args()

    print(args)

    # Main loop (connect/reconnect)
    while True:
        try:
            ser = serial.Serial(args.port)  # open serial port
            print('Opening:' + ser.name)

            # Data loop Until disconnected (exception) grab and send data
            while True:
                json_key={}
                cpu_usage(json_key, args.delay)
                cpu_temp(json_key)
                percent_mem(json_key)
                disk_usage(json_key)
                #print(json.dumps(json_key), end='\r')
                data = json.dumps(json_key) 
                ser.write(data.encode('ascii'))

        except serial.SerialException as e:
            print('Disconnected: ' + str(e))

        time.sleep(5)

if __name__ == "__main__":
    main()
