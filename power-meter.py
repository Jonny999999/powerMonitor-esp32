# code for testing / programming the power converter module PZEM-004T
# https://de.aliexpress.com/item/1005004382390344.html
from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusException
import time
import serial

def read_data(client, slave):
    result = client.read_input_registers(address=0x0000, count=10, slave=slave)
    if result.isError():
        print("Read error:", result)
        return
    regs = result.registers
    print("Raw Registers:", regs)

    voltage = regs[0] / 10.0
    current = ((regs[1] << 16) + regs[2]) / 1000.0
    power   = ((regs[3] << 16) + regs[4]) / 10.0

    energy_high = regs[5]
    energy_low = regs[6]
    energy = (energy_high << 16) | energy_low

    frequency = regs[7] / 10.0
    pf = regs[8] / 100.0

    print(f"Voltage: {voltage} V")
    print(f"Current: {current} A")
    print(f"Power: {power} W")
    print(f"Energy: {energy} Wh")
    print(f"Frequency: {frequency} Hz")
    print(f"Power Factor: {pf}")



def set_slave_address(client, current_slave, new_slave):
    # This writes a single register at 0x0002 with the new address (1–247)
    print(f"Setting address from {current_slave} to {new_slave}...")
    result = client.write_register(address=0x0002, value=new_slave, slave=current_slave)
    if result.isError():
        print("Failed to set address:", result)
    else:
        print("Address changed. Power cycle the PZEM module if needed.")



def crc16(data: bytes):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if (crc & 0x0001):
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def reset_energy_pzem(port="/dev/ttyUSB0", slave_address=0x01):
    # Construct command: [slave_address, 0x42]
    payload = bytes([slave_address, 0x42])
    crc = crc16(payload)
    crc_low = crc & 0xFF
    crc_high = (crc >> 8) & 0xFF
    frame = payload + bytes([crc_low, crc_high])

    # Send over serial
    with serial.Serial(port, baudrate=9600, timeout=1) as ser:
        ser.write(frame)
        time.sleep(0.1)
        # Read response (optional, most versions return nothing or echo the frame)
        response = ser.read(4)
        print("Reset Command Sent.")
        print(f"Raw response: {response.hex()}")




# -----------------------------
# Configuration
port = '/dev/ttyUSB0'
slave = 1  # Current slave address


# -----------------------------
# Setup serial client
client = ModbusSerialClient(
    port=port,
    baudrate=9600,
    parity='N',
    stopbits=1,
    bytesize=8,
    timeout=1
)


if client.connect():
    try:
        ########## RUN DESIRED FUNCTION ##########
        # read all registers:
        read_data(client, slave)

        # set slave address:
        #set_slave_address(client, current_slave=slave, new_slave=1)

        # reset energy:
        #reset_energy_pzem(port, slave)

        pass  # remove this after choosing an action above

    except ModbusException as e:
        print("Modbus error:", e)
    finally:
        client.close()
else:
    print("Failed to connect to Modbus client.")
