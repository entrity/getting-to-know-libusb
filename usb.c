/*
	http://libusb.sourceforge.net/api-1.0/modules.html

	http://www.usbmadesimple.co.uk/ums_7.htm#high_speed_isoc_trans
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <signal.h>

#define DEBUG
#define VID 0x0582
#define PID 0x0073

libusb_device ** list;
libusb_device_handle *handle;
static int aborted = 0;
FILE * outfile;

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
	libusb_device * out = NULL;
	struct libusb_device_descriptor desc;
	ssize_t i = 0;
	ssize_t n = libusb_get_device_list(NULL, &list);
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
	fatal(dev == NULL, "no dev for debug fn");
	int ret, i, j, k, l;
	struct libusb_device_descriptor desc;
	ret = libusb_get_device_descriptor(dev, &desc);
	fatal(ret, "get device descriptor");
	
	printf("\tconf | intf | alts | edpt :: eaddr\n");

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
					printf("\t%4d | %4d | %4d | %4d :: %02x %d :: %d\n", i, j, k, l, endpoint->bEndpointAddress, endpoint->bEndpointAddress, max_size);
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

#define NUM_ISO_PACKETS 10
#define PKT_SIZE 192

static void capture_callback(struct libusb_transfer *transfer)
{
	int i;
	printf("cap cb stat %d\n", transfer->status);

	for (i = 0; i < NUM_ISO_PACKETS; i++) {
		struct libusb_iso_packet_descriptor *desc =
			&transfer->iso_packet_desc[i];
		unsigned char *pbuf = transfer->buffer + (i * PKT_SIZE);
		if (desc->status != 0) {
			printf("packet %d has status %d\n", i, desc->status);
			continue;
		}
		if (desc->actual_length != PKT_SIZE)
			printf("unexpected data length %d vs %d\n", desc->actual_length, PKT_SIZE);
		// fwrite(transfer->buffer + (i * PKT_SIZE), 1, desc->actual_length, outfile);
		write(1, transfer->buffer + (i * PKT_SIZE), desc->actual_length);
	}
	if (!aborted) libusb_submit_transfer(transfer);
}


static struct libusb_transfer *alloc_capture_transfer(void)
{
	int bufflen = PKT_SIZE * NUM_ISO_PACKETS;
	int i;
	struct libusb_transfer *transfer = libusb_alloc_transfer(NUM_ISO_PACKETS);

	fatal(!transfer, "transfer alloc failure");
	transfer->dev_handle = handle;
	transfer->endpoint = 0x01;
	transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
	transfer->timeout = 5000;
	transfer->buffer = malloc(bufflen);
	transfer->length = bufflen;
	transfer->callback = capture_callback;
	transfer->num_iso_packets = NUM_ISO_PACKETS;
	
	for (i = 0; i < NUM_ISO_PACKETS; i++) {
		transfer->iso_packet_desc[i].length = PKT_SIZE;
	}
	return transfer;
}

/*
Endpoint Descriptor:
    bLength                 9
    bDescriptorType         5
    bEndpointAddress     0x01  EP 1 OUT
    bmAttributes            9
      Transfer Type            Isochronous
      Synch Type               Adaptive
      Usage Type               Data
    wMaxPacketSize     0x00c0  1x 192 bytes
    bInterval               1
    bRefresh                0
    bSynchAddress           0
    AudioControl Endpoint Descriptor:
      bLength                 7
      bDescriptorType        37
      bDescriptorSubtype      1 (EP_GENERAL)
      bmAttributes         0x00
      bLockDelayUnits         2 Decoded PCM samples
      wLockDelay            512 Decoded PCM samples
*/
void read_dev()
{
	int r;
	// get handle
	handle = libusb_open_device_with_vid_pid(NULL, VID, PID);
	// claim interface, connect
	int intf_i = 1;
	int alts_i = 1;
	r = libusb_detach_kernel_driver(handle, intf_i); // okay to fail b/c the driver may already have been detached
	r = libusb_claim_interface(handle, intf_i);
	fatal(r, "libusb_claim_interface");
	r = libusb_set_interface_alt_setting(handle, intf_i, alts_i);
	fatal(r, "libusb_set_interface_alt_setting");
	// open outfile
	fopen("raw.pcm", "wb");
	// submit transfers
	struct libusb_transfer *tx1 = alloc_capture_transfer();
	struct libusb_transfer *tx2 = alloc_capture_transfer();
	r = libusb_submit_transfer(tx1);
	fatal(r, "transfer submit 1");
	r = libusb_submit_transfer(tx2);
	fatal(r, "transfer submit 2");

	// wait until xfers complete
	while (!aborted)
		if (libusb_handle_events(NULL) < 0)
			break;

	// release
	close(outfile);
	r = libusb_set_interface_alt_setting(handle, intf_i, 0);
	fatal(r, "libusb_set_interface_alt_setting 0");
	libusb_close(handle);
}


static void sighandler(int signum)
{
	printf("got signal %d\n", signum);
	aborted = 1;
}

int main(int argc, char * argv[])
{
	int rc;
	#ifdef DEBUG
	printf("starting\n");
	#endif

	// signal handling
	struct sigaction sigact;
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	// init libusb
	rc = libusb_init(NULL);
	fatal(rc, "libusb_init");

	// diag
	debug(find_dev(VID, PID));

	// take action
	read_dev();

	// de-init libusb
	libusb_exit(NULL);
	return 0;
}
