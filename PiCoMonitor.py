#!/usr/bin/env python3
# coding=utf-8

import psutil, json
import time
import serial
import sys

def disk_usage(json_key):
    hdd = psutil.disk_partitions()
    data = []
    for each in hdd:
        path = each.mountpoint
        fstype = each.fstype
        # filter devices
        if sys.platform.startswith("linux"):
            # filter devices
            if fstype != "ext4":
                continue
            if path.startswith("/var/"):
                continue

            label = path
            if path.startswith("/mnt/"):
                label = path.replace("/mnt/", "")
        else:
            if fstype != "NTFS":
                continue
            if fstype == "":
                continue
            label = path

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

def cpu_usage(json_key):
    cpu_usage = psutil.cpu_percent(interval=0.5, percpu=True)
    json_key['CPU'] = cpu_usage

def cpu_temp(json_key):
    if sys.platform.startswith("linux"):
        temps = psutil.sensors_temperatures()
        json_key['TEMP'] = temps['amdgpu'][0][1]
    else:
        json_key['TEMP'] = 0.0

def percent_mem(json_key):
    mem_percent = psutil.virtual_memory().percent
    json_key['RAM'] = mem_percent


while True:
    try:
        if sys.platform.startswith("linux"):
            ser = serial.Serial('/dev/ttyACM0')  # open serial port
        else:
            ser = serial.Serial('COM3')  # open serial port
        print('Opening:' + ser.name)

        while True:
            json_key={}
            cpu_usage(json_key)
            cpu_temp(json_key)
            percent_mem(json_key)
            disk_usage(json_key)
            #print(json.dumps(json_key), end='\r')
            data = json.dumps(json_key) 
            ser.write(data.encode('ascii'))

    except serial.SerialException as e:
        print('Disconnected: ' + str(e))

    time.sleep(5)

    
