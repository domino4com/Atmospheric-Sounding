
import io
import locale
import logging
import operator
import pprint
import signal
import sys
import traceback
from functools import reduce
import xml.dom.minidom

import pynmea2  # pip install pynmea2
import serial  # pip install pyserial

pp = pprint.PrettyPrinter(indent=4)

def handler(signum, frame):
    print("...Ended")
    sys.exit(0)


signal.signal(signal.SIGINT, handler)

ser = serial.Serial(sys.argv[1], 115200)

while True:
    try:
        nmeadata = ser.readline().strip().decode() #.strip("$\n").split("*", 1)
        msg = pynmea2.parse(nmeadata,check=True)
        if hasattr(msg, "latitude"):
            # print(msg.timestamp,msg.latitude,msg.longitude,msg.altitude,msg.geo_sep,msg.age_gps_data,msg.ref_station_id)
            lat=msg.latitude
            lon=msg.longitude
            alt=msg.altitude

        elif hasattr(msg, "true_track"):
            # print(msg.true_track,msg.spd_over_grnd_kts,msg.spd_over_grnd_kmph)
            cog=msg.true_track

        doc = xml.dom.minidom.Document()
        doc.appendChild(root := doc.createElement("kml"))
        root.setAttribute('xmlns', "http://www.opengis.net/kml/2.2")
        root.appendChild(camera := doc.createElement("Camera"))
   
        camera.appendChild(latitude := doc.createElement("latitude"))
        latitude.appendChild(doc.createTextNode(f'{lat:.6f}'))

        camera.appendChild(longitude := doc.createElement('longitude'))
        longitude.appendChild(doc.createTextNode(f'{lon:.6f}'))

        camera.appendChild(altitude := doc.createElement('altitude'))
        altitude.appendChild(doc.createTextNode(f'{alt:.3f}'))

        camera.appendChild(altitudeMode := doc.createElement('altitudeMode'))
        altitudeMode.appendChild(doc.createTextNode("absolute"))

        camera.appendChild(heading := doc.createElement('heading'))
        heading.appendChild(doc.createTextNode(f'{cog:.3f}'))

        camera.appendChild(tilt := doc.createElement('tilt'))
        tilt.appendChild(doc.createTextNode("45"))

        xml_data = doc.toprettyxml(
            indent='  ',
            newl='\n',
            encoding='utf8',
        ).decode('utf8')
        print(xml_data)
        f = open(sys.argv[2], "a")
        f.write(xml_data)
        f.close()

    except Exception as e:
        logging.error(traceback.format_exc())
        print("Data Error: " + nmeadata, "Error:"+str(e))
        continue