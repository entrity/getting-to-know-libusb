#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define DEBUG

libusb_device ** list;

void fatal(int code, char * msg)
{
	if (code) {
		dprintf(2, "ERR %d : %s\n", code, msg);
		exit(code);
	}
}

libusb_device * open_dev(libusb_device * dev)
{
	int ret;
	libusb_device_handle * handle = NULL;
	ret = libusb_open(dev, &handle);
	fatal(LIBUSB_ERROR_ACCESS == ret, "insufficient permissions");
	fatal(LIBUSB_ERROR_NO_DEVICE == ret, "no device; this represents my program bug probably");
	fatal(LIBUSB_ERROR_NO_MEM == ret, "no memory!");
	if (LIBUSB_SUCCESS == ret) {
		#ifdef DEBUG
		char buff[256];
		struct libusb_device_descriptor desc;
		ret = libusb_get_device_descriptor(dev, &desc);
		fatal(ret, "get device descriptor");
		printf("dev %04x:%04x\n", desc.idVendor, desc.idProduct);
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, buff, sizeof(buff));
			fatal(ret == 0, "mfr");
			printf("Manufacturer : %s\n", buff);
		}
		#endif
	}
}

/* Open USB device and return */
libusb_device * find_dev(int vid, int pid)
{
	ssize_t i = 0;
	ssize_t n = libusb_get_device_list(NULL, &list);
	libusb_device * out = NULL;
	struct libusb_device_descriptor desc;
	fatal(!n, "Count is 0");
	printf("count is %lu\n", n);
	for (i = 0; i < n; i++) {
		printf("%lu\n", i);
		libusb_get_device_descriptor(list[i], &desc);
		if (desc.idVendor == vid && desc.idProduct == pid) {
			out = list[i];
			#ifdef DEBUG
			printf("%s %lu\n", "found device!", i);
			printf("Dev (bus %d, device %d)\n", libusb_get_bus_number(out), libusb_get_device_address(out));
			#endif
			break;
		}
	}
	libusb_free_device_list(list, 1);
	return out;
}

static void print(libusb_device * dev)
{
	struct libusb_device_descriptor desc;
	libusb_device_handle * handle = NULL;
	char description[256];
	char string[256];
	int ret, i;

	fatal(dev == NULL, "no dec");

	ret = libusb_get_device_descriptor(dev, &desc);
	fatal(ret, "get device descriptor");
	
	ret = libusb_open(dev, &handle);
	fatal(LIBUSB_ERROR_ACCESS == ret, "insufficient permissions");
	fatal(LIBUSB_ERROR_NO_DEVICE == ret, "no device; this represents my program bug probably");
	fatal(LIBUSB_ERROR_NO_MEM == ret, "no memory!");
	
	if (LIBUSB_SUCCESS == ret) {
		printf("dev %04x:%04x\n", desc.idVendor, desc.idProduct);
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			fatal(ret == 0, "mfr");
			printf("%s\n", string);
		}
	}
	if (handle) libusb_close(handle);
	
}

void ls()
{
	ssize_t i = 0;
	ssize_t n = libusb_get_device_list(NULL, &list);
	fatal(!n, "Count is 0");

	printf("%lu devs found\n", (unsigned long) n);

	for (i = 0; i < n; i++) {
		libusb_device * dev = list[i];
		print(dev);
	}
	libusb_free_device_list(list, 1);
}

int main(int argc, char * argv[])
{
	int rc;
	printf("starting\n");
	// init libusb
	rc = libusb_init(NULL);
	fatal(rc, "libusb_init");
	// diag
	open_dev(find_dev(0x0582, 0x0073));
	// ls();
	// de-init libusb
	libusb_exit(NULL);
	return 0;
}