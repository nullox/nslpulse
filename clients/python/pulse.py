#!/usr/bin/python3

# Script Name   : pulse.py
# Author        : William Johnson <johnson@nullox.com>
# Created       : 15th May 2017
# Last Modified	: 15th May 2017
# Version       : 1.0
# Description   : probe pulses of nsl servers in the wild

# python pulse.py PATH_1, PATH_2, PATH_3 (remember to append hash to path)

import sys
import urllib.request

# argument check for path(s)
if len(sys.argv) < 2:
    print('path(s) parameters are missing')
    sys.exit()

paths = sys.argv[1].split(',')
for path in paths:
    if not "/" in path:
        print('invalid form found in paths')
        sys.exit()

print("NSL Pulse CLI Reporting Client")

for path in paths:
    try:
        path = path.strip()
        print("\nprobing "+ path)
        if not path[:4] == 'http':
            path = 'http://' + path
        with urllib.request.urlopen(path) as response:
            metrics = response.read().decode('utf-8')
            metrics = metrics.split(':')
            if len(metrics) == 9:
                host = path
                host = host.replace('https://', '')
                host = host.replace('http://', '')
                host = host.split('/')
                host = host[0]

                print("\nis alive...")
                print('host: '+ host)
                print('cpu: '+ metrics[0])
                print('database: ' + 'up' if metrics[1] == '1' else 'down')
                print('uptime: '+ metrics[2] +' second(s)')
                print('free disk: '+ str(round(float(metrics[4])/1048576,2)) +'gb')
                print('total disk: '+ str(round(float(metrics[3])/1048576,2)) +'gb')
                print('free ram: '+ str(round(float(metrics[7])/1048576,2)) +'gb')
                print('total ram: '+ str(round(float(metrics[6])/1048576,2)) +'gb')
                print('disk usage: '+ metrics[5])
                print('ram usage: '+ metrics[8])

            else:
                print('failed...')

    except urllib2.HTTPError:
        print('pulse error for '+ path +', HTTPError')
    except urllib2.URLError:
        print('pulse error for '+ path +', URLError')
    except urllib2.HTTPException:
        print('pulse error for '+ path +', HTTPException')
    except Exception:
        print('generic exception whilst probing '+ path)
