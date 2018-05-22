#!/usr/bin/env python

from spritz.spritz import Spritz
import sys


key = bytearray('aaaaaaaaaa111111')
data = open(sys.argv[1], 'rb').read()

data = bytearray(data[:len(data) - len(data)%16])

spritz = Spritz()

a = spritz.encrypt(key, bytearray('a'*100))
a += spritz.encrypt(key, bytearray('b'*10))

print str(a).encode('hex')

print spritz.decrypt(key, a)

#print spritz.decrypt(key, spritz.encrypt(key, bytearray("a" * 1024)))
#print spritz.decrypt(key, data)



