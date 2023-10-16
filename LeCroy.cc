/*
 * T. Straumann 2023, based on work of hansiglaser.
 */

/*
 * C-port of https://github.com/hansiglaser/pas-gpib/blob/master/usb/usblecroy.pas
 */

#include <libusb-1.0/libusb.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdexcept>
#include <memory>

#include <LeCroy.h>

#define CTRL_INTF_NUMBER 0
#define DATA_INTF_NUMBER 0

#define _STR_(x) # x
#define _STR(x) _STR_(x)

#define BUFSZ_HS (16*65536)
#define BUFSZ_FS ( 2*65536)

#define TOTSZ_HS (100*1024*1024)
#define TOTSZ_FS (2*1024*1024)

#define TIMEOUT_MS 1000

int
LeCroy::ctl_msg(uint8_t *buf, size_t len)
{
	int st = libusb_control_transfer(
	             m_devh, 
	             LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
	             0x0b,
	             0,
	             0,
	             buf,
	             len,
	             m_timeout
	         );
	if ( st <= 0 ) {
		throw std::runtime_error( std::string( "control transfer failed: " ) + std::to_string( st ) );
	}
	return st;
}

struct B {
	uint8_t *m_p;

	B() : m_p(0) {
	}

	~B() {
		::free( m_p );
	}
};

int
LeCroy::vctl_msg(int m0, ...)
{
	va_list                  ap;
	int                      narg = 0;
	int                      i;
	int                      st   = -1;
	B                        buf;

	if ( m0 >= 0 ) {
		narg++;

		va_start( ap, m0 );
		while ( va_arg(ap, int) >= 0 )
			narg++;
		va_end( ap );

		if ( ! (buf.m_p = (uint8_t*)malloc( narg )) )
			throw std::runtime_error("no memory");

		buf.m_p[0] = m0;

		va_start( ap, m0 );
		for ( i = 1; i < narg; i++ ) {
			buf.m_p[i] = va_arg( ap, int );
		}
		va_end( ap );
	}

	st = ctl_msg( buf.m_p, narg );
	return st;
}

int
LeCroy::snd_str( const char *buf )
{
	int put, st;
	st = libusb_bulk_transfer( m_devh, m_wendp, (uint8_t*)buf, strlen(buf), &put, m_timeout );
	if ( st < 0 ) {
		throw std::runtime_error( std::string( "snd_str: bulk transfer failed: " ) + std::to_string( st ) );
	}
	return put;
}

int
LeCroy::rcv_str( char *buf, size_t bufsz )
{
	int st;
	int got;
	int got1;
	unsigned char crlf[2];
	st = libusb_bulk_transfer( m_devh, m_rendp, (uint8_t*)buf, bufsz, &got, m_timeout );
	if ( st < 0 ) {
		throw std::runtime_error( std::string( "rcv_str: bulk transfer failed: " ) + std::to_string( st ) );
	}
	st = libusb_bulk_transfer( m_devh, m_rendp, crlf, sizeof(crlf), &got1, m_timeout );
	if ( st < 0 ) {
		throw std::runtime_error( std::string( "rcv_str: crlf bulk transfer failed: " ) + std::to_string( st ) );
	}
	return got;
}

class UsbCfg {
private:
	struct libusb_config_descriptor *m_cfg;
public:
	UsbCfg(struct libusb_config_descriptor *cfg)
	: m_cfg( cfg )
	{
	}

	~UsbCfg()
	{
		libusb_free_config_descriptor( m_cfg );
	}
};

LeCroy::LeCroy(int vid, int pid)
: m_devh   (  0         ),
  m_ctx    (  0         ),
  m_intf   ( -1         ),
  m_wendp  ( -1         ),
  m_rendp  ( -1         ),
  m_timeout( TIMEOUT_MS )
{
	struct libusb_config_descriptor                 *cfg   = 0;
	const struct libusb_endpoint_descriptor         *e     = 0;

	try {

		int st = libusb_init( &m_ctx );
		if ( st ) {
			throw std::runtime_error( std::string( "libusb_init: " ) + std::to_string(st) );
		}

		m_devh = libusb_open_device_with_vid_pid( m_ctx, vid, pid );
		if ( ! m_devh ) {
			throw std::runtime_error( "libusb_open_device_with_vid_pid: not found" );
		}

		if ( libusb_set_auto_detach_kernel_driver( m_devh, 1 ) ) {
			throw std::runtime_error( "libusb_set_auto_detach_kernel_driver: failed" );
		}

		st = libusb_get_active_config_descriptor( libusb_get_device( m_devh ), &cfg );
		if ( st ) {
			throw std::runtime_error( std::string( "libusb_get_active_config_descriptor: " ) + std::to_string( st ));
		}

		// used just for cleanup;
		UsbCfg guard( cfg );

		if ( cfg->bNumInterfaces <= DATA_INTF_NUMBER ) {
			throw std::runtime_error( "unexpected number of interfaces!");
		}

		if ( cfg->interface[DATA_INTF_NUMBER].altsetting[0].bInterfaceClass !=  0 ) {
			throw std::runtime_error( "unexpected interface class (not 0)");
		}
		if ( cfg->interface[DATA_INTF_NUMBER].altsetting[0].bInterfaceSubClass !=  0 ) {
			throw std::runtime_error( "unexpected interface subclass (not 0)");
		}

		st = libusb_claim_interface( m_devh, DATA_INTF_NUMBER );
		if ( st ) {
			throw std::runtime_error( std::string( "libusb_claim_interface: " ) + std::to_string(st) );
		}

		e = cfg->interface[DATA_INTF_NUMBER].altsetting[0].endpoint;
		for (int i = 0; i < cfg->interface[DATA_INTF_NUMBER].altsetting[0].bNumEndpoints; i++, e++ ) {
			if ( LIBUSB_TRANSFER_TYPE_BULK != (LIBUSB_TRANSFER_TYPE_MASK & e->bmAttributes) ) {
				continue;
			}
			if ( LIBUSB_ENDPOINT_DIR_MASK & e->bEndpointAddress ) {
				m_rendp = e->bEndpointAddress;
			} else {
				m_wendp = e->bEndpointAddress;
			}
		}

		if ( m_rendp < 0 || m_wendp < 0 ) {
			throw std::runtime_error( "Unable to find (both) bulk endpoints");
		}

		vctl_msg( 0x03, 0x01, -1);
		vctl_msg( 0x01, 0x03, -1);
		vctl_msg( 0x00, 0x08, 0x01, -1);   // $05: 19200, $08: 115200
		vctl_msg( 0x04, 0x00, 0x00, -1);
		vctl_msg( 0x01, 0x03, -1);
		vctl_msg( 0x01, 0x03, -1);
		vctl_msg( 0x00, 0x08, 0x01, -1);   // $05: 19200, $08: 115200
		vctl_msg( 0x04, 0x00, 0x00, -1);
		vctl_msg( 0x02, 0x11, 0x13, -1);
		vctl_msg( 0x04, 0x00, 0x00, -1);
		vctl_msg( 0x00, 0x08, 0x11, -1);   // $05: 19200, $08: 115200
		vctl_msg( 0x04, 0x00, 0x00, -1);

		libusb_clear_halt( m_devh, m_wendp );
		libusb_clear_halt( m_devh, m_rendp );

	} catch ( std::runtime_error & ) {
		cleanup();
		throw;
	}
}

void
LeCroy::cleanup()
{
	if ( m_devh ) {
		if ( m_intf >= 0 ) {
			libusb_release_interface( m_devh, m_intf );
		}
		libusb_close( m_devh );
	}
	if ( m_ctx ) {
		libusb_exit( m_ctx );
	}
}

LeCroy::~LeCroy()
{
	cleanup();
}
