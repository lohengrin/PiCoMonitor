#!/usr/bin/env python3
# coding=utf-8

import psutil, json
import time
import serial

def cpu_usage(json_key):
    cpu_usage = psutil.cpu_percent(interval=0.5, percpu=True)
    json_key['CPU'] = cpu_usage

def cpu_temp(json_key):
    temps = psutil.sensors_temperatures()
    json_key['TEMP'] = temps['amdgpu'][0][1]

def percent_mem(json_key):
    mem_percent = psutil.virtual_memory().percent
    json_key['RAM'] = mem_percent

while True:
    try:
        ser = serial.Serial('/dev/ttyACM0')  # open serial port
        print('Opening:' + ser.name)

        while True:
            json_key={}
            cpu_usage(json_key)
            cpu_temp(json_key)
            percent_mem(json_key)
            print(json.dumps(json_key))

            data = json.dumps(json_key) 
            ser.write(data.encode('ascii'))

    except serial.SerialException as e:
        print('Disconnected: ' + str(e))

    time.sleep(5)

    
