/*
 * T. Straumann 2023, based on work of hansiglaser.
 */

/*
 * C-port of https://github.com/hansiglaser/pas-gpib/blob/master/usb/usblecroy.pas
 */

#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#define CTRL_INTF_NUMBER 0
#define DATA_INTF_NUMBER 0

#define ID_VEND 0x05ff
#define ID_PROD 0x0021

#define _STR_(x) # x
#define _STR(x) _STR_(x)

#define BUFSZ_HS (16*65536)
#define BUFSZ_FS ( 2*65536)

#define TOTSZ_HS (100*1024*1024)
#define TOTSZ_FS (2*1024*1024)

#define TIMEOUT_MS 1000

static void usage(const char *nm, int lvl)
{
    printf("usage: %s\n", nm);
}

class LeCroy {

static int
ctl_msg(libusb_device_handle *devh, uint8_t *buf, size_t len)
{
	int st = libusb_control_transfer(
	             devh, 
	             LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
	             0x0b,
	             0,
	             0,
	             buf,
	             len,
	             TIMEOUT_MS
	         );
	if ( st <= 0 ) {
		fprintf(stderr, "control transfer failed: %d\n", st);
	}
	return st;
}

static int
vctl_msg(libusb_device_handle *devh, ...)
{
	va_list  ap;
	int      narg = 0;
	int      i;
	uint8_t *buf  = 0;
	int      st   = -1;

	va_start( ap, devh );
	while ( va_arg(ap, int) >= 0 )
		narg++;
	va_end( ap );

	if ( ! (buf = malloc( narg )) )
		goto bail;

	va_start( ap, devh );
	for ( i = 0; i < narg; i++ ) {
		buf[i] = va_arg( ap, int );
	}
	va_end( ap );

	st = ctl_msg( devh, buf, narg );
bail:
	free( buf );
	return st;
}

int
snd_str( libusb_device_handle *devh, int ep, char *buf)
{
	int got, st;
	st = libusb_bulk_transfer( devh, ep, buf, strlen(buf), &got, TIMEOUT_MS );
	if ( st < 0 ) {
		fprintf(stderr, "snd_str: bulk transfer failed: %d\n", st);
	}
	return st;
}

int
rcv_str( libusb_device_handle *devh, int ep, char *buf, size_t bufsz )
{
	int st;
	int got;
	int got1;
	char crlf[2];
	st = libusb_bulk_transfer( devh, ep, buf, bufsz, &got, TIMEOUT_MS );
	if ( st < 0 ) {
		fprintf(stderr, "rcv_str: bulk transfer failed: %d\n", st);
		return st;
	}
	st = libusb_bulk_transfer( devh, ep, crlf, sizeof(crlf), &got1, TIMEOUT_MS );
	if ( st < 0 ) {
		fprintf(stderr, "rcv_str: crlf bulk transfer failed: %d\n", st);
		return st;
	}
	return got;
}

int
main(int argc, char **argv)
{
libusb_device_handle                            *devh  = 0;
libusb_context                                  *ctx   = 0;
int                                              rv    = 1;
int                                              intf  = -1;
int                                              st;
struct libusb_config_descriptor                 *cfg   = 0;
const struct libusb_endpoint_descriptor         *e     = 0;
int                                              rendp = -1;
int                                              wendp = -1;
int                                              xendp;
int                                              i, got;
unsigned char                                    buf[BUFSZ_HS];
int                                              opt;
int                                              len  =  0;
int                                             *i_p;
int                                              fill = -1;
int                                              oneo = -1;
int                                              head = -1;
int                                              wr   =  0;
int                                              vid  = ID_VEND;
int                                              pid  = ID_PROD;
struct timespec                                  then, now;
double                                           diff;
int                                              help = -1;
int                                              timeout_sec = 1000;
unsigned long                                    tot, totl   = 0;
unsigned long                                   *l_p;
enum libusb_speed                                spd;

	while ( (opt = getopt(argc, argv, "h")) > 0 ) {
		i_p = 0;
		l_p = 0;
		switch (opt)  {
			case 'h':  help++;        break;
			default:
				fprintf(stderr, "Error: Unknown option -%c\n", opt);
				usage( argv[0], 0 );
				goto bail;
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr, "Unable to scan option -%c arg\n", opt);
			goto bail;
		}
		if ( l_p && 1 != sscanf(optarg, "%li", l_p) ) {
			fprintf(stderr, "Unable to scan option -%c arg\n", opt);
			goto bail;
		}
	}

	if ( help >= 0 ) {
		usage( argv[0], help );
		return 0;
	}

	st = libusb_init( &ctx );
	if ( st ) {
		fprintf(stderr, "libusb_init: %i\n", st);
		goto bail;
	}

	devh = libusb_open_device_with_vid_pid( ctx, vid, pid );
	if ( ! devh ) {
		fprintf(stderr, "libusb_open_device_with_vid_pid: not found\n");
		goto bail;
	}

	if ( libusb_set_auto_detach_kernel_driver( devh, 1 ) ) {
		fprintf(stderr, "libusb_set_auto_detach_kernel_driver: failed\n");
		goto bail;
	}

	st = libusb_get_active_config_descriptor( libusb_get_device( devh ), &cfg );
	if ( st ) {
		fprintf(stderr, "libusb_get_active_config_descriptor: %i\n", st);
		goto bail;
	}

	if ( cfg->bNumInterfaces <= DATA_INTF_NUMBER ) {
		fprintf(stderr, "unexpected number of interfaces!\n");
		goto bail;
	}

	if ( cfg->interface[DATA_INTF_NUMBER].altsetting[0].bInterfaceClass !=  0 ) {
		fprintf(stderr, "unexpected interface class (not 0)\n");
		goto bail;
	}
	if ( cfg->interface[DATA_INTF_NUMBER].altsetting[0].bInterfaceSubClass !=  0 ) {
		fprintf(stderr, "unexpected interface subclass (not 0)\n");
		goto bail;
	}

	st = libusb_claim_interface( devh, DATA_INTF_NUMBER );
	if ( st ) {
		fprintf(stderr, "libusb_claim_interface: %i\n", st);
		goto bail;
	}

	e = cfg->interface[DATA_INTF_NUMBER].altsetting[0].endpoint;
	for ( i = 0; i < cfg->interface[DATA_INTF_NUMBER].altsetting[0].bNumEndpoints; i++, e++ ) {
		if ( LIBUSB_TRANSFER_TYPE_BULK != (LIBUSB_TRANSFER_TYPE_MASK & e->bmAttributes) ) {
			continue;
		}
		if ( LIBUSB_ENDPOINT_DIR_MASK & e->bEndpointAddress ) {
			rendp = e->bEndpointAddress;
		} else {
			wendp = e->bEndpointAddress;
		}
	}

	if ( rendp < 0 || wendp < 0 ) {
		fprintf(stderr, "Unable to find (both) bulk endpoints\n");
		goto bail;
	}

	if ( vctl_msg( devh, 0x03, 0x01, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x01, 0x03, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x00, 0x08, 0x01, -1) <= 0 ) goto bail;   // $05: 19200, $08: 115200
	if ( vctl_msg( devh, 0x04, 0x00, 0x00, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x01, 0x03, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x01, 0x03, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x00, 0x08, 0x01, -1) <= 0 ) goto bail;   // $05: 19200, $08: 115200
	if ( vctl_msg( devh, 0x04, 0x00, 0x00, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x02, 0x11, 0x13, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x04, 0x00, 0x00, -1) <= 0 ) goto bail;
	if ( vctl_msg( devh, 0x00, 0x08, 0x11, -1) <= 0 ) goto bail;   // $05: 19200, $08: 115200
	if ( vctl_msg( devh, 0x04, 0x00, 0x00, -1) <= 0 ) goto bail;

	printf("clear halt OUT %d\n", libusb_clear_halt( devh, wendp ) );
	printf("clear halt IN  %d\n", libusb_clear_halt( devh, rendp ) );

	if ( snd_str( devh, wendp, "*IDN?\n\r" ) < 0 ) goto bail;

	printf("RX: %d\n", (got = rcv_str( devh, rendp, buf, sizeof(buf) )) );
	buf[got] = 0;
	printf("ANS:%s\n", buf);
	
	rv = 0;

bail:
	if ( cfg ) {
		libusb_free_config_descriptor( cfg );
	}
	if ( devh ) {
		if ( intf >= 0 ) {
			libusb_release_interface( devh, intf );
		}
		libusb_close( devh );
	}
	if ( ctx ) {
		libusb_exit( ctx );
	}
	return rv;
}
