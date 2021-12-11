from pymodbus.client.sync import ModbusSerialClient as ModbusClient

modbus = ModbusClient(method='rtu', port='/dev/tty.wchusbserial14130', baudrate=9600, timeout=10)
modbus.connect()
test = modbus.read_input_registers(address = 0x0001,count = 0x02, unit=0x08)
print(test.registers)

# change address
#res = modbus.write_register(address = 0x0101 , value = 0x08, unit=0x09)
