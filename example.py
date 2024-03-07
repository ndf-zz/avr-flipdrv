# SPDX-License-Identifier: MIT

# Example clock for flipdot display

import serial
from time import sleep
from datetime import datetime

p = serial.Serial('/dev/ttyUSB0', 9600, rtscts=False)
p.write(b'\x07 [\x08\x08> CK\n')
lt = None
while True:
    nt = datetime.now()
    if nt.minute != lt:
        if nt.minute == 0:
            p.write(b'\x07')
        p.write(nt.strftime('%H %M\n').encode('ascii'))
        lt = nt.minute
    sleep(5)
