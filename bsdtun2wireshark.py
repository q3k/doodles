# Copyright (c) 2015, Sergiusz Bazanski <sergiusz@bazanski.pl>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#  this list of conditions and the following disclaimer in the documentation
#  and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# This scripts allows you to view OpenBSD tun tcpdump files on a Linux
# Wireshark instance.
# What seems to be happening is that OpenBSD's tcpdump produces a DLT of
# 0x0C, and then sticks a four byte header in front of each packet. We undo
# this :)

import sys
import struct


if len(sys.argv) < 3:
    sys.stderr.write("Usage: {} <in.pcap> <out.pcap>\n".format(sys.argv[0]))
    sys.stderr.flush()
    sys.exit(1)

ifile = open(sys.argv[1])
ofile = open(sys.argv[2], 'w')

# Find endianness of input file
magic, = struct.unpack('<I', ifile.read(4))
if magic == 0xa1b2c3d4:
    endian = '<'
elif magic == 0xd4c3b2a1:
    endian = '>'
else:
    sys.stderr.write("This does not look like PCAP file.\n")
    sys.exit(1)

# Copy headers over
ofile.write(struct.pack(endian + 'I', magic))
for _ in range(4):
    ofile.write(ifile.read(4))
dlt, = struct.unpack(endian + 'I', ifile.read(4))
if dlt != 0x0c:
    sys.stderr.write("Was expecting DLT of type 0x0C, got 0x{:x} instead."
                     .format(dlt))
    sys.exit(1)
ofile.write(struct.pack(endian + 'I', 101))

# Copy packets over
packet_count = 0
while True:
    ts = ifile.read(4)
    if len(ts) == 0:
        # Natural end-of-file reached
        break
    ts_sec,  = struct.unpack(endian + 'I', ts)
    ts_usec, incl_len, orig_len = struct.unpack(endian + 'III',
                                                ifile.read(12))
    # Get rid of the first four bytes
    incl_len -= 4
    orig_len -= 4
    ifile.read(4)

    # Write new header and data
    ofile.write(struct.pack(endian + 'IIII', ts_sec, ts_usec, incl_len, orig_len))
    ofile.write(ifile.read(incl_len))
    packet_count += 1

ofile.close()
ifile.close()

sys.stderr.write("{} packets written.\n".format(packet_count))
sys.stderr.flush()
