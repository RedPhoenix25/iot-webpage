import serial
import time

try:
    ser = serial.Serial()
    ser.port = 'COM7'
    ser.baudrate = 115200
    ser.setDTR(False)
    ser.setRTS(False)
    ser.open()
    t_end = time.time() + 10
    print("Reading from COM7 without reset...")
    while time.time() < t_end:
        line = ser.readline()
        if line:
            print(line.decode('utf-8', 'ignore').strip())
    ser.close()
except Exception as e:
    print(f"Error: {e}")
