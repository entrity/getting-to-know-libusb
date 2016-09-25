
c: usb.c
	gcc usb.c -I/usr/include/libusb-1.0 -lusb-1.0 -o usb.out

run:
	sudo ./usb.out
# all: usb.cpp
# 	g++ usb.cpp -I/usr/include/libusb-1.0 -lusb-1.0 -o usb.out
