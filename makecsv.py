import serial
import traceback
import logging
import sys
import operator
from functools import reduce
import signal


def handler(signum, frame):
    print('...Ended')
    exit(0)


signal.signal(signal.SIGINT, handler)

ser = serial.Serial(sys.argv[1], 115200)

csvline = ""

while True:
    try:
        nmeadata, last = ser.readline().strip().decode().strip("$\n").split('*', 1)
        cksum, rssi, snr = last.split(',', 3)
        calc_cksum = reduce(operator.xor, (ord(s) for s in nmeadata), 0)
    except Exception as e:
        logging.error(traceback.format_exc())
        print("Data Error: " + nmeadata)
        continue

    try:
        if int(cksum, 16) == calc_cksum:
            # Lora: https://lora.readthedocs.io/en/latest/
            # print(rssi,snr) # print RSSI and SNR rssi=-60 is good, rssi=-120 is weak, snr=10 is good,  snr=-20 is bad
            if nmeadata.startswith("GPGGA"):
                _, time, _, _, _, _, q, sats, hdop, alt, * \
                    rest = csvline.split(',')
                if q == "0":
                    qs = "Fix not available or invalid - DO NOT LAUNCH"
                elif q == "1":
                    qs = "GPS SPS Mode, fix valid"
                elif q == "2":
                    qs = "Differential GPS, SPS Mode, or Satellite Based Augmentation System (SBAS), fix valid"
                elif q == "3":
                    qs = "GPS PPS Mode, fix valid"
                elif q == "4":
                    qs = "Real Time Kinematic (RTK) System used in RTK mode with fixed integers"
                elif q == "5":
                    qs = "Float RTK. Satellite system used in RTK mode, floating integers"
                elif q == "6":
                    qs = "Estimated (dead reckoning) mode"
                else:
                    qs = "Unknown"
                if qs != "Unknown":
                    f = open(sys.argv[2], "a")
                    f.write(csvline+'\n')
                    f.close()
                print("Time:"+time[0:2]+":"+time[2:4]+":"+time[4:6], "Q:"+q,
                      "Sats#:"+sats, "HDOP:"+hdop, "Alt:"+alt, "RSSI:"+rssi, "SNR:"+snr, qs)
                csvline = nmeadata
            else:
                csvline = csvline + "," + nmeadata
        else:
            print("Checksum error: " + nmeadata)
    except Exception as e:
        logging.error(traceback.format_exc())
        print("Error: " + nmeadata)

