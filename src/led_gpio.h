/////////////////////////////////////////////////////////////////////////////
/// @file led_gpio.h
///
/// LED control via Linux GPIO (libgpiod)
///
/// Directly drives GPIO pins on boards like Raspberry Pi, eliminating the
/// need for an intermediary Arduino/Pico. Uses software PWM for brightness.
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
#ifndef INCLUDED_LED_GPIO
#define INCLUDED_LED_GPIO

//- includes
#include "led_control_base.h"
#include "mediasmartserverd.h"

#include <cstring>
#include <iostream>
#include <string>

#ifdef HAVE_LIBGPIOD
#include <gpiod.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/////////////////////////////////////////////////////////////////////////////
/// LED control via Linux GPIO with software PWM
class LedGpio : public LedControlBase {
public:
	/// constructor
	explicit LedGpio( const std::string& chip_path, const int* pin_map )
		: chip_( 0 )
		, chip_path_( chip_path )
		, brightness_( 9 )
		, pwm_running_( false )
		, sys_blue_blink_( false )
		, sys_red_blink_( false )
		, blink_phase_( false )
	{
		memset( lines_, 0, sizeof(lines_) );
		memset( led_state_, 0, sizeof(led_state_) );

		for ( int i = 0; i < NUM_PINS; ++i ) {
			pins_[i] = pin_map[i];
		}
	}

	/// destructor
	virtual ~LedGpio( ) {
		// stop PWM thread
		if ( pwm_running_ ) {
			pwm_running_ = false;
			pthread_join( pwm_thread_, 0 );
		}

		// turn off all LEDs and release lines
		for ( int i = 0; i < NUM_PINS; ++i ) {
			if ( lines_[i] ) {
				gpiod_line_set_value( lines_[i], 0 );
				gpiod_line_release( lines_[i] );
			}
		}
		if ( chip_ ) gpiod_chip_close( chip_ );
	}

	/// description
	virtual const char* Desc( ) const { return "Linux GPIO LEDs"; }

	/// initialize - open gpiochip and request pins
	virtual bool Init( ) {
		chip_ = gpiod_chip_open( chip_path_.c_str() );
		if ( !chip_ ) {
			std::cerr << "GPIO: failed to open " << chip_path_ << "\n";
			return false;
		}

		// request all pins as outputs
		for ( int i = 0; i < NUM_PINS; ++i ) {
			if ( pins_[i] < 0 ) continue; // skip unconfigured pins

			lines_[i] = gpiod_chip_get_line( chip_, pins_[i] );
			if ( !lines_[i] ) {
				std::cerr << "GPIO: failed to get line " << pins_[i] << "\n";
				cleanup_();
				return false;
			}
			if ( gpiod_line_request_output( lines_[i], "mediasmartserverd", 0 ) < 0 ) {
				std::cerr << "GPIO: failed to request line " << pins_[i] << " as output\n";
				cleanup_();
				return false;
			}
		}

		// start software PWM thread
		pwm_running_ = true;
		if ( pthread_create( &pwm_thread_, 0, pwmThreadFunc_, this ) != 0 ) {
			std::cerr << "GPIO: failed to create PWM thread\n";
			pwm_running_ = false;
			cleanup_();
			return false;
		}

		if ( verbose > 0 ) {
			std::cout << "GPIO: initialized on " << chip_path_ << " pins:";
			for ( int i = 0; i < NUM_PINS; ++i ) {
				if ( pins_[i] >= 0 ) std::cout << " " << pins_[i];
			}
			std::cout << "\n";
		}

		return true;
	}

	/// set bay LED state
	virtual void Set( int led_type, size_t led_idx, bool state ) {
		if ( led_idx >= MAX_HDD_LEDS ) return;
		if ( led_type & LED_BLUE ) led_state_[PIN_BAY_BLUE_0 + led_idx] = state;
		if ( led_type & LED_RED  ) led_state_[PIN_BAY_RED_0  + led_idx] = state;
	}

	/// set global brightness (0-9)
	virtual void SetBrightness( int val ) {
		if ( val < 0 ) val = 0;
		if ( val > 9 ) val = 9;
		brightness_ = val;
	}

	/// set system LED state
	virtual void SetSystemLed( int led_type, LedState state ) {
		if ( led_type & LED_BLUE ) {
			led_state_[PIN_SYS_BLUE] = ( state == LED_ON || state == LED_BLINK );
			sys_blue_blink_ = ( state == LED_BLINK );
		}
		if ( led_type & LED_RED ) {
			led_state_[PIN_SYS_RED] = ( state == LED_ON || state == LED_BLINK );
			sys_red_blink_ = ( state == LED_BLINK );
		}
	}

	/// mount/unmount USB indicator
	virtual void MountUsb( bool state ) {
		led_state_[PIN_USB] = state;
	}

	//- pin index constants
	enum {
		PIN_BAY_BLUE_0 = 0,  // bay 0 blue
		PIN_BAY_BLUE_1 = 1,  // bay 1 blue
		PIN_BAY_BLUE_2 = 2,  // bay 2 blue
		PIN_BAY_BLUE_3 = 3,  // bay 3 blue
		PIN_BAY_RED_0  = 4,  // bay 0 red
		PIN_BAY_RED_1  = 5,  // bay 1 red
		PIN_BAY_RED_2  = 6,  // bay 2 red
		PIN_BAY_RED_3  = 7,  // bay 3 red
		PIN_SYS_BLUE   = 8,  // system blue
		PIN_SYS_RED    = 9,  // system red
		PIN_USB        = 10, // USB indicator
		NUM_PINS       = 11,
	};

private:
	static const size_t MAX_HDD_LEDS = 4;

	gpiod_chip*  chip_;
	gpiod_line*  lines_[NUM_PINS];
	int          pins_[NUM_PINS];        ///< GPIO pin numbers
	bool         led_state_[NUM_PINS];   ///< logical on/off state
	std::string  chip_path_;
	int          brightness_;            ///< 0-9
	pthread_t    pwm_thread_;
	volatile bool pwm_running_;

	// blink state for system LEDs
	volatile bool sys_blue_blink_;
	volatile bool sys_red_blink_;
	volatile bool blink_phase_;

	/// clean up on failed init
	void cleanup_( ) {
		for ( int i = 0; i < NUM_PINS; ++i ) {
			if ( lines_[i] ) {
				gpiod_line_release( lines_[i] );
				lines_[i] = 0;
			}
		}
		if ( chip_ ) {
			gpiod_chip_close( chip_ );
			chip_ = 0;
		}
	}

	/// software PWM thread entry point
	static void* pwmThreadFunc_( void* arg ) {
		LedGpio* self = static_cast<LedGpio*>( arg );
		self->pwmLoop_();
		return 0;
	}

	/// software PWM loop
	/// Runs at 1kHz (1ms per step), 10 steps per cycle = 100Hz PWM
	/// Also handles blink timing for system LEDs
	void pwmLoop_( ) {
		int step = 0;
		unsigned long blink_counter = 0;

		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000; // 1ms

		while ( pwm_running_ ) {
			int bright = brightness_;

			for ( int i = 0; i < NUM_PINS; ++i ) {
				if ( !lines_[i] ) continue;

				bool logical_on = led_state_[i];

				// handle system LED blink
				if ( i == PIN_SYS_BLUE && sys_blue_blink_ && !blink_phase_ ) logical_on = false;
				if ( i == PIN_SYS_RED  && sys_red_blink_  && !blink_phase_ ) logical_on = false;

				// USB LED: no PWM, just on/off
				if ( i == PIN_USB ) {
					gpiod_line_set_value( lines_[i], logical_on ? 1 : 0 );
					continue;
				}

				// software PWM: pin is on for 'bright' steps out of 10
				bool hw_on = logical_on && ( step < bright );
				gpiod_line_set_value( lines_[i], hw_on ? 1 : 0 );
			}

			// advance PWM step (0-9)
			if ( ++step >= 10 ) step = 0;

			// blink toggle every 500 cycles (500ms at 1ms/cycle)
			if ( ++blink_counter >= 500 ) {
				blink_counter = 0;
				if ( sys_blue_blink_ || sys_red_blink_ ) {
					blink_phase_ = !blink_phase_;
				}
			}

			nanosleep( &ts, 0 );
		}

		// turn off all pins on exit
		for ( int i = 0; i < NUM_PINS; ++i ) {
			if ( lines_[i] ) gpiod_line_set_value( lines_[i], 0 );
		}
	}
};

#else // !HAVE_LIBGPIOD

/////////////////////////////////////////////////////////////////////////////
/// Stub when libgpiod is not available
class LedGpio : public LedControlBase {
public:
	explicit LedGpio( const std::string&, const int* ) { }
	virtual ~LedGpio( ) { }
	virtual const char* Desc( ) const { return "Linux GPIO LEDs (unavailable)"; }
	virtual bool Init( ) {
		std::cerr << "GPIO driver: not compiled with libgpiod support\n";
		return false;
	}
	virtual void Set( int, size_t, bool ) { }
	virtual void SetBrightness( int ) { }
	virtual void SetSystemLed( int, LedState ) { }
	virtual void MountUsb( bool ) { }

	enum { NUM_PINS = 11 };
};

#endif // HAVE_LIBGPIOD

#endif // INCLUDED_LED_GPIO
