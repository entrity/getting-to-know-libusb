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

libusb_device_handle * open_dev(libusb_device * dev)
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
	return LIBUSB_SUCCESS == ret ? handle : NULL;
}

/* Open USB device and return */
libusb_device * find_dev(int vid, int pid)
{
	ssize_t i = 0;
	ssize_t n = libusb_get_device_list(NULL, &list);
	libusb_device * out = NULL;
	struct libusb_device_descriptor desc;
	fatal(!n, "Count is 0");
	#ifdef DEBUGV
	printf("count is %lu\n", n);
	#endif
	for (i = 0; i < n; i++) {
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

void handle_dev(libusb_device_handle *handle)
{

}

/* Diagnostic utility */
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

/* Diagnostic utility */
void debug(libusb_device * dev)
{
	int ret, i, j, k, l;
	struct libusb_device_descriptor desc;
	ret = libusb_get_device_descriptor(dev, &desc);
	fatal(ret, "get device descriptor");
	
	printf("\tconf | intf | altset | endpt :: eaddr\n");

	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct libusb_config_descriptor *config;
		ret = libusb_get_config_descriptor(dev, i, &config);
		fatal(ret, "libusb_get_config_descriptor");
		for (j = 0; j < config->bNumInterfaces; j++) {
			struct libusb_interface *interface = &config->interface[j];
			for (k = 0; k < interface->num_altsetting; k++) {
				struct libusb_interface_descriptor *idesc = &interface->altsetting[k];
				for (l = 0; l < idesc->bNumEndpoints; l++) {
					struct libusb_endpoint_descriptor *endpoint = &idesc->endpoint[l];
					int max_size = libusb_get_max_iso_packet_size(dev, endpoint->bEndpointAddress);
					printf("\t %d | %d | %d | %d :: %02x %d :: %d\n", i, j, k, l, endpoint->bEndpointAddress, endpoint->bEndpointAddress, max_size);
				}
			}
		}
	}
	
}

/* Diagnostic utility */
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
	#ifdef DEBUG
	printf("starting\n");
	#endif
	// init libusb
	rc = libusb_init(NULL);
	fatal(rc, "libusb_init");
	// diag

	// libusb_device_handle * handle = libusb_open_device_with_vid_pid(NULL, 0x0582, 0x0073);
	// handle_dev(handle);
	debug(find_dev(0x0582, 0x0073));

	// de-init libusb
	libusb_exit(NULL);
	return 0;
}