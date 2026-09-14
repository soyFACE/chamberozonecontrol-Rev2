import serial  # This is the "pyserial" module which is bizarrely imported as "serial". The module named "serial" will not work.
import io
import time
from datetime import datetime
from pprint import pprint


com_port = 'COM4'
rate = 9600
timeout = 0.25
counter = 0
ozone_state = 0

start_hour = 10
start_minute = 00
end_hour = 16
end_minute = 00

conn = serial.Serial(com_port, rate, timeout=timeout)
serial_io = io.TextIOWrapper(io.BufferedRWPair(conn, conn))

serial_log_file_name = datetime.now().strftime("%Y%m%d-%H%M%S") + "_serial_log.csv"



with open(serial_log_file_name, 'w') as serial_log:
    time.sleep(3)
    while True:
        now = datetime.now()
        start_time = now
        start_time = start_time.replace(hour=start_hour, minute=start_minute, second=0, microsecond=0)
        end_time = start_time.replace(hour=end_hour, minute=end_minute, second=0, microsecond=0)
        if now >= start_time and now < end_time and ozone_state == 0 :
            ozone_state = 1
            print("entered")
            conn.write(b'<O,1>')
        if now >= end_time and ozone_state == 1:
            ozone_state = 0
            conn.write(b'<O,0>')
        while True:
           lines = serial_io.readlines()
           n_lines = len(lines)
           #print(len(lines))
           if n_lines >= 1: break  # It can take some time for the data to appear. Keep reading the buffer until there is something.
        #print(lines)
        with open(serial_log_file_name, 'a') as serial_log:
           for line in lines:
               print("Datetime:" + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + "," + line)
               serial_log.write("Datetime:" + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + "," + line)
