#!/usr/bin/python
# coding=utf-8

import psutil
import time

while True:
    CPU_usage = psutil.cpu_percent(interval=1, percpu=True)
    ram = psutil.virtual_memory()
    ram_free = ram.free // 2**20

    print(str(len(CPU_usage)) + ":" + str(CPU_usage))
    print(ram_free)
    
    time.sleep(1)
    
