from pymodbus.client import ModbusSerialClient

client = ModbusSerialClient(
    port='/dev/ttyUSB0',
    baudrate=9600,
    parity='N',
    stopbits=1,
    bytesize=8,
    timeout=1
)

client.connect()

result = client.read_input_registers(address=0x0000, count=10, slave=1)

if result.isError():
    print("Read error:", result)
else:
    regs = result.registers
    print("Raw Registers:", regs)

    voltage = regs[0] / 10.0
    current = ((regs[1] << 16) + regs[2]) / 1000.0
    power   = ((regs[3] << 16) + regs[4]) / 10.0
    energy  = (regs[5] << 16) + regs[6]
    frequency = regs[7] / 10.0
    pf = regs[8] / 100.0

    print(f"Voltage: {voltage} V")
    print(f"Current: {current} A")
    print(f"Power: {power} W")
    print(f"Energy: {energy} Wh")
    print(f"Frequency: {frequency} Hz")
    print(f"Power Factor: {pf}")

client.close()
