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

def output_pulse_string(metrics_t, path_t):
    metrics_t = metrics_t.split(':')
    if len(metrics_t) == 9:
        host = path_t
        host = host.replace('https://', '')
        host = host.replace('http://', '')
        host = host.split('/')
        host = host[0]
        print("\nis alive...")
        print('host: '+ host)
        print('cpu: '+ metrics_t[0])
        print('database: ' + 'up' if metrics_t[1] == '1' else 'down')
        print('uptime: '+ metrics_t[2] +' second(s)')
        print('free disk: '+ str(round(float(metrics_t[4])/1048576,2)) +'gb')
        print('total disk: '+ str(round(float(metrics_t[3])/1048576,2)) +'gb')
        print('free ram: '+ str(round(float(metrics_t[7])/1048576,2)) +'gb')
        print('total ram: '+ str(round(float(metrics_t[6])/1048576,2)) +'gb')
        print('disk usage: '+ metrics_t[5])
        print('ram usage: '+ metrics_t[8])
    else:
        print('metric string cannot be read...')

# is path via http...
def is_path_http(path_t):
    return '/' in path_t

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
    if is_path_http(path):
        try:
            path = path.strip()
            print("\nprobing "+ path)
            if not path[:4] == 'http':
                path = 'http://' + path
            with urllib.request.urlopen(path) as response:
                metrics = response.read().decode('utf-8')
                output_pulse_string(metrics, path)
        except urllib2.HTTPError:
            print('pulse error for '+ path +', HTTPError')
        except urllib2.URLError:
            print('pulse error for '+ path +', URLError')
        except urllib2.HTTPException:
            print('pulse error for '+ path +', HTTPException')
        except Exception:
            print('generic exception whilst probing '+ path)
    else:
        print('non http')
