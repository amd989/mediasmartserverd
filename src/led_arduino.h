/////////////////////////////////////////////////////////////////////////////
/// @file led_arduino.h
///
/// LED control via Arduino USB serial
///
/// Communicates with an Arduino running the mediasmartserver_leds firmware
/// over a USB serial connection. The Arduino drives the actual LED GPIOs.
///
/// -------------------------------------------------------------------------
///
/// Copyright (c) 2026 Alejandro Mora
///
/// This software is provided 'as-is', without any express or implied
/// warranty. In no event will the authors be held liable for any damages
/// arising from the use of this software.
///
/// Permission is granted to anyone to use this software for any purpose,
/// including commercial applications, and to alter it and redistribute it
/// freely, subject to the following restrictions:
///
/// 1. The origin of this software must not be misrepresented; you must not
/// claim that you wrote the original software. If you use this software
/// in a product, an acknowledgment in the product documentation would be
/// appreciated but is not required.
///
/// 2. Altered source versions must be plainly marked as such, and must not
/// be misrepresented as being the original software.
///
/// 3. This notice may not be removed or altered from any source
/// distribution.
///
/////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDED_LED_ARDUINO
#define INCLUDED_LED_ARDUINO

//- includes
#include "led_control_base.h"
#include "mediasmartserverd.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

/////////////////////////////////////////////////////////////////////////////
/// LED control via Arduino over USB serial
class LedArduino : public LedControlBase {
public:
	/// constructor
	explicit LedArduino( const std::string& serial_port )
		: fd_( -1 )
		, serial_port_( serial_port )
	{ }

	/// destructor - reset LEDs and close port
	virtual ~LedArduino( ) {
		if ( fd_ >= 0 ) {
			sendCommand_( "R\n" );
			close( fd_ );
			fd_ = -1;
		}
	}

	/// description
	virtual const char* Desc( ) const { return "Arduino USB Serial LEDs"; }

	/// initialize - open serial port and verify connection
	virtual bool Init( ) {
		fd_ = openSerial_( );
		if ( fd_ < 0 ) return false;

		// Pro Micro resets on serial open (DTR). Wait for it to boot.
		usleep( 2500000 ); // 2.5 seconds

		// Flush any boot messages
		tcflush( fd_, TCIOFLUSH );

		// Ping the Arduino
		if ( !sendCommand_( "P\n" ) ) {
			std::cerr << "Arduino: failed to send ping\n";
			close( fd_ );
			fd_ = -1;
			return false;
		}

		// Wait for "OK" response
		char buf[16];
		memset( buf, 0, sizeof(buf) );

		fd_set rfds;
		FD_ZERO( &rfds );
		FD_SET( fd_, &rfds );
		struct timeval tv = { 3, 0 }; // 3 second timeout

		int ret = select( fd_ + 1, &rfds, 0, 0, &tv );
		if ( ret > 0 ) {
			int n = read( fd_, buf, sizeof(buf) - 1 );
			if ( n > 0 ) {
				// Strip trailing whitespace
				while ( n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r') ) buf[--n] = '\0';
				if ( strcmp( buf, "OK" ) == 0 ) {
					if ( verbose > 0 ) std::cout << "Arduino: connected on " << serial_port_ << "\n";
					return true;
				}
				std::cerr << "Arduino: unexpected response: " << buf << "\n";
			}
		} else {
			std::cerr << "Arduino: ping timeout on " << serial_port_ << "\n";
		}

		close( fd_ );
		fd_ = -1;
		return false;
	}

	/// set bay LED state
	virtual void Set( int led_type, size_t led_idx, bool state ) {
		if ( led_idx >= MAX_HDD_LEDS ) return;
		char cmd[16];
		if ( led_type & LED_BLUE ) {
			snprintf( cmd, sizeof(cmd), "S B %u %d\n", (unsigned)led_idx, state ? 1 : 0 );
			sendCommand_( cmd );
		}
		if ( led_type & LED_RED ) {
			snprintf( cmd, sizeof(cmd), "S R %u %d\n", (unsigned)led_idx, state ? 1 : 0 );
			sendCommand_( cmd );
		}
	}

	/// set global brightness (0-9)
	virtual void SetBrightness( int val ) {
		if ( val < 0 ) val = 0;
		if ( val > 9 ) val = 9;
		char cmd[8];
		snprintf( cmd, sizeof(cmd), "B %d\n", val );
		sendCommand_( cmd );
	}

	/// set system LED state
	virtual void SetSystemLed( int led_type, LedState state ) {
		int state_val = 0;
		if ( state == LED_ON    ) state_val = 1;
		if ( state == LED_BLINK ) state_val = 2;

		char cmd[16];
		if ( (led_type & LED_BLUE) && (led_type & LED_RED) ) {
			snprintf( cmd, sizeof(cmd), "Y A %d\n", state_val );
			sendCommand_( cmd );
		} else if ( led_type & LED_BLUE ) {
			snprintf( cmd, sizeof(cmd), "Y B %d\n", state_val );
			sendCommand_( cmd );
		} else if ( led_type & LED_RED ) {
			snprintf( cmd, sizeof(cmd), "Y R %d\n", state_val );
			sendCommand_( cmd );
		}
	}

	/// mount/unmount USB indicator
	virtual void MountUsb( bool state ) {
		char cmd[8];
		snprintf( cmd, sizeof(cmd), "U %d\n", state ? 1 : 0 );
		sendCommand_( cmd );
	}

private:
	static const size_t MAX_HDD_LEDS = 4;

	int         fd_;           ///< serial port file descriptor
	std::string serial_port_;  ///< serial port path

	/// open and configure the serial port
	int openSerial_( ) {
		int fd = open( serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK );
		if ( fd < 0 ) {
			std::cerr << "Arduino: failed to open " << serial_port_ << ": " << strerror(errno) << "\n";
			return -1;
		}

		// Clear non-blocking for normal operation
		int flags = fcntl( fd, F_GETFL, 0 );
		fcntl( fd, F_SETFL, flags & ~O_NONBLOCK );

		struct termios tty;
		memset( &tty, 0, sizeof(tty) );
		if ( tcgetattr( fd, &tty ) != 0 ) {
			std::cerr << "Arduino: tcgetattr failed: " << strerror(errno) << "\n";
			close( fd );
			return -1;
		}

		cfsetispeed( &tty, B115200 );
		cfsetospeed( &tty, B115200 );

		tty.c_cflag = CS8 | CLOCAL | CREAD;  // 8N1, ignore modem control, enable read
		tty.c_iflag = IGNPAR;                 // ignore parity errors
		tty.c_oflag = 0;                      // raw output
		tty.c_lflag = 0;                      // raw input (no echo, no canonical)

		tty.c_cc[VMIN]  = 0;   // non-blocking read
		tty.c_cc[VTIME] = 10;  // 1 second read timeout

		// Prevent DTR reset on close (helps with Pro Micro)
		tty.c_cflag &= ~HUPCL;

		tcflush( fd, TCIOFLUSH );
		if ( tcsetattr( fd, TCSANOW, &tty ) != 0 ) {
			std::cerr << "Arduino: tcsetattr failed: " << strerror(errno) << "\n";
			close( fd );
			return -1;
		}

		return fd;
	}

	/// send a command string to the Arduino
	bool sendCommand_( const char* cmd ) {
		if ( fd_ < 0 ) {
			// Attempt reconnect
			fd_ = openSerial_( );
			if ( fd_ < 0 ) return false;
			usleep( 2500000 );
			tcflush( fd_, TCIOFLUSH );
		}

		size_t len = strlen( cmd );
		ssize_t n = write( fd_, cmd, len );
		if ( n < 0 ) {
			if ( debug ) std::cerr << "Arduino: write failed: " << strerror(errno) << "\n";
			// Try to reconnect once
			close( fd_ );
			fd_ = openSerial_( );
			if ( fd_ < 0 ) return false;
			usleep( 2500000 );
			tcflush( fd_, TCIOFLUSH );
			n = write( fd_, cmd, len );
			if ( n < 0 ) {
				std::cerr << "Arduino: write failed after reconnect: " << strerror(errno) << "\n";
				close( fd_ );
				fd_ = -1;
				return false;
			}
		}
		return true;
	}
};

#endif // INCLUDED_LED_ARDUINO
