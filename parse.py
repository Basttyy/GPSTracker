#!/usr/bin/env python2
#-*- coding: utf-8 -*-
import pynmea2
import argparse
import sys
from gmplot import gmplot
from Crypto.Cipher import AES

entrylist = []
minlat = 1000
maxlat = -1000
minlon = 1000
maxlon = -1000
CHUNKSIZE = 16


def parsepos(line):
    global minlat, minlon, maxlat, maxlon
    pos = pynmea2.parse(line)
    entry = (pos.latitude, pos.longitude)
    if pos.latitude == 0 and pos.longitude == 0:
        return
    if minlat > entry[0]: minlat = entry[0]
    if minlon > entry[1]: minlon = entry[1]
    if maxlat < entry[0]: maxlat = entry[0]
    if maxlon < entry[1]: maxlon = entry[1]
    entrylist.append(entry)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--logfile', type=argparse.FileType('r'), default=sys.stdin)
    parser.add_argument('--output', required=True)
    parser.add_argument('--password', help='password', required=True)
    args = parser.parse_args()
    decipher = AES.new(args.password, AES.MODE_CBC, args.password)#FIXME: Random IV
    cleartext = ''
    while True:
        enc = args.logfile.read(CHUNKSIZE)
        if len(enc) == 0:
            break
        cleartext += decipher.decrypt(enc)
        if '\n' in cleartext:
            line, cleartext = cleartext.split('\n', 1)
            date, pos = line.split(';', 1)
            parsepos(pos)
    #PARSE HERE
    gmap = gmplot.GoogleMapPlotter(minlat, minlon, max(maxlat - minlat, maxlon - minlon))
    a, b = zip(*entrylist)
    gmap.plot(a, b, 'cornflowerblue', edge_width=10)
    gmap.draw(args.output)
