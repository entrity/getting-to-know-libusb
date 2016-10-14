/*
	http://libusb.sourceforge.net/api-1.0/modules.html

	http://www.usbmadesimple.co.uk/ums_7.htm#high_speed_isoc_trans
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include <signal.h>
#include <time.h>

#define DEBUG
#define VID 0x0582
#define PID 0x0073
#define EPT 0x82
#define ITF 2
#define ALT 1
#define NUM_ISO_PACKETS 10
#define PKT_SIZE 192

libusb_device ** list;
libusb_device_handle *handle;
static int aborted = 0;
FILE * outfile;
timer_t timerid;

void fatal(int code, char * msg, int line)
{
	if (code) {
		dprintf(2, "ERR %d : %d : %s\n", code, line, msg);
		exit(code);
	}
}

void timer_callback(union sigval val)
{
	aborted = 1;
	printf("Timer fired\n");
}

static void capture_callback(struct libusb_transfer *transfer)
{
	int i;
	// printf("cap cb stat %d\n", transfer->status);

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
		fwrite(transfer->buffer + (i * PKT_SIZE), desc->actual_length, 1, outfile);
		// write(1, transfer->buffer + (i * PKT_SIZE), desc->actual_length);
	}
	if (!aborted) libusb_submit_transfer(transfer);
}


static struct libusb_transfer *alloc_capture_transfer(void)
{
	int bufflen = PKT_SIZE * NUM_ISO_PACKETS;
	int i;
	struct libusb_transfer *transfer = libusb_alloc_transfer(NUM_ISO_PACKETS);

	fatal(!transfer, "transfer alloc failure", __LINE__);
	transfer->dev_handle = handle;
	transfer->endpoint = EPT;
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
	fatal(handle == NULL, "no matching device found", __LINE__);
	// claim interface, connect
	int intf_i = ITF;
	int alts_i = ALT;
	r = libusb_detach_kernel_driver(handle, intf_i); // okay to fail b/c the driver may already have been detached
	r = libusb_claim_interface(handle, intf_i);
	fatal(r, "libusb_claim_interface", __LINE__);
	r = libusb_set_interface_alt_setting(handle, intf_i, alts_i);
	fatal(r, "libusb_set_interface_alt_setting", __LINE__);
	// open outfile
	outfile = fopen("raw.pcm", "wb");
	// submit transfers
	struct libusb_transfer *tx1 = alloc_capture_transfer();
	struct libusb_transfer *tx2 = alloc_capture_transfer();
	r = libusb_submit_transfer(tx1);
	fatal(r, "transfer submit 1", __LINE__);
	r = libusb_submit_transfer(tx2);
	fatal(r, "transfer submit 2", __LINE__);

	// wait until xfers complete
	while (!aborted)
		if (libusb_handle_events(NULL) < 0)
			break;

	// release
	fclose(outfile);
	r = libusb_set_interface_alt_setting(handle, intf_i, 0);
	fatal(r, "libusb_set_interface_alt_setting 0", __LINE__);
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

	// timer
	struct sigevent sige;
	struct itimerspec its;
	sige.sigev_notify = SIGEV_THREAD;
	sige.sigev_notify_function = &timer_callback;
	rc = timer_create(CLOCK_REALTIME, &sige, &timerid);
	fatal(rc, "timer_create failed", __LINE__);
	its.it_value.tv_sec = its.it_interval.tv_sec = 1;
	its.it_value.tv_nsec = its.it_interval.tv_nsec = 0;
	rc = timer_settime(timerid, 0, &its, NULL);

	// init libusb
	rc = libusb_init(NULL);
	fatal(rc, "libusb_init", __LINE__);

	// take action
	read_dev();

	// de-init libusb
	libusb_exit(NULL);
	return 0;
}
