/*
 * T. Straumann 2023, based on work of hansiglaser.
 */

/*
 * C-port of https://github.com/hansiglaser/pas-gpib/blob/master/usb/usblecroy.pas
 */

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdexcept>

#include <LeCroy.h>

#define _STR_(x) # x
#define _STR(x) _STR_(x)

#define BUFSZ_HS (16*65536)
#define BUFSZ_FS ( 2*65536)

#define TOTSZ_HS (100*1024*1024)
#define TOTSZ_FS (2*1024*1024)

static void usage(const char *nm, int lvl)
{
    printf("usage: %s\n", nm);
}

int
main(int argc, char **argv)
{
int                                              rv    = 1;
int                                              got;
char                                             buf[BUFSZ_HS];
int                                              opt;
int                                             *i_p;
int                                              vid  = LeCroy::ID_VEND;
int                                              pid  = LeCroy::ID_PROD;
int                                              help = -1;
unsigned long                                   *l_p;

	while ( (opt = getopt(argc, argv, "h")) > 0 ) {
		i_p = 0;
		l_p = 0;
		switch (opt)  {
			case 'h':  help++;        break;
			default:
				fprintf(stderr, "Error: Unknown option -%c\n", opt);
				usage( argv[0], 0 );
				return 1;
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr, "Unable to scan option -%c arg\n", opt);
			return 1;
		}
		if ( l_p && 1 != sscanf(optarg, "%li", l_p) ) {
			fprintf(stderr, "Unable to scan option -%c arg\n", opt);
			return 1;
		}
	}

	if ( help >= 0 ) {
		usage( argv[0], help );
		return 0;
	}

	LeCroy l(vid, pid);

	l.snd_str( "*IDN?\n\r" );
	printf("RX: %d\n", (got = l.rcv_str( buf, sizeof(buf) )) );
	buf[got] = 0;
	printf("ANS:%s\n", buf);
	
	rv = 0;

	return rv;
}
