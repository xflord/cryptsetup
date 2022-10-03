#!/bin/sh

head -c 52M /dev/zero > luksfile
sudo losetup -f luksfile --show > device.txt
sudo cryptsetup luksFormat --type luks1 `cat device.txt`

gcc -g -O -c test.c
libtool --mode=link gcc -g -O -o test test.o /usr/lib/libcryptsetup.la
./test

sudo losetup -d `cat device.txt`

rm luksfile device.txt test test.o
