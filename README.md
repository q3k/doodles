q3k's bits and doodles
======================

Variosu scripts that don't merit their own repository.

bsdtun2wireshark
----------------

This scripts allows you to view OpenBSD tun tcpdump files on a Linux Wireshark instance.
What seems to be happening is that OpenBSD's tcpdump produces a DLT of 0x0C, and then sticks a four byte header in front of each packet. We undo this :)

