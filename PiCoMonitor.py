#!/usr/bin/python
# coding=utf-8

import psutil
import time

while True:
    CPU_usage = psutil.cpu_percent(interval=1, percpu=True)
    ram = psutil.virtual_memory()
    sensors = psutil.sensors_temperatures()

    print("CPU:" + str(len(CPU_usage)) + " " + str(CPU_usage))
    print("RAM:" + str(ram))
    print("TEM:" + str(sensors))
    
    time.sleep(1)
    
