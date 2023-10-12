/*
 * T. Straumann 2023, based on work of hansiglaser.
 */

/*
 * C-port of https://github.com/hansiglaser/pas-gpib/blob/master/usb/usblecroy.pas
 */

#ifndef LE_CROY_USB_H
#define LE_CROY_USB_H

#include <libusb-1.0/libusb.h>

class LeCroy {
private:
	libusb_device_handle *m_devh;
	libusb_context       *m_ctx;
	int                   m_intf;
	int                   m_wendp;
	int                   m_rendp;
	int                   m_timeout;

	LeCroy(const LeCroy&);
	LeCroy &operator=(const LeCroy&);

	void cleanup();

public:

	static const int ID_VEND = 0x05ff;
	static const int ID_PROD = 0x0021;

	int
	ctl_msg(uint8_t *buf, size_t len);
	// va list of bytes, -1 terminated
	int
	vctl_msg(int m0, ...);

	int
	snd_str( const char *buf );

	int
	rcv_str( char *buf, size_t bufsz );

	LeCroy(int vid=ID_VEND, int pid=ID_PROD);
	~LeCroy();
};

#endif
