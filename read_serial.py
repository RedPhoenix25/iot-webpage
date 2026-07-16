import serial
import time

try:
    ser = serial.Serial()
    ser.port = 'COM7'
    ser.baudrate = 115200
    ser.setDTR(False)
    ser.setRTS(False)
    ser.open()
    
    # Toggle RTS and DTR to hardware reset the ESP32
    print("Resetting ESP32...")
    ser.setRTS(True)
    ser.setDTR(True)
    time.sleep(0.2)
    ser.setRTS(False)
    ser.setDTR(False)
    time.sleep(0.5)
    
    t_end = time.time() + 15
    print("Reading from COM7...")
    while time.time() < t_end:
        line = ser.readline()
        if line:
            print(line.decode('utf-8', 'ignore').strip())
    ser.close()
except Exception as e:
    print(f"Error: {e}")
