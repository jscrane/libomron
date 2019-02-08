/*
 * Declaration file for Omron Health User Space Driver
 *
 * Copyright (c) 2009-2010 Kyle Machulis <kyle@nonpolynomial.com>
 *
 * More info on Nonpolynomial Labs @ http://www.nonpolynomial.com
 *
 * Sourceforge project @ http://www.github.com/qdot/libomron/
 *
 * This library is covered by the BSD License
 * Read LICENSE_BSD.txt for details.
 */

#ifndef LIBOMRON_H
#define LIBOMRON_H

/*******************************************************************************
 *
 * Headers
 *
 ******************************************************************************/

#define OMRON_ERR_DEVIO   (-1)
#define OMRON_ERR_BADARG  (-2)
#define OMRON_ERR_NOTOPEN (-3)
#define OMRON_ERR_BUFSIZE (-4)
#define OMRON_ERR_NEGRESP (-5)
#define OMRON_ERR_ENDRESP (-6)
#define OMRON_ERR_BADDATA (-7)

#define OMRON_DEBUG_ERROR   1
#define OMRON_DEBUG_WARNING 2
#define OMRON_DEBUG_INFO    3
#define OMRON_DEBUG_DETAIL  4
#define OMRON_DEBUG_PROTO   5
#define OMRON_DEBUG_DEVIO   6

#include <stdint.h>

#if defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(OMRON_DYNAMIC)
#define OMRON_DECLSPEC __declspec(dllexport)
#else
#define OMRON_DECLSPEC
#endif

/**
 * Structure to hold information about Windows HID devices.
 *
 * @ingroup CoreFunctions
 */
typedef struct {
	/// Windows device handle
	HANDLE _dev;
	/// 0 if device is closed, > 0 otherwise
	int _is_open;
} omron_device_impl;
#else
#define OMRON_DECLSPEC
#include "libusb-1.0/libusb.h"
typedef struct {
	struct libusb_context* _context;
	struct libusb_device_handle* _device;
	int _is_open;
} omron_device_impl;
#endif

/*******************************************************************************
 *
 * Const global values
 *
 ******************************************************************************/

/// Vendor ID for all omron health devices
extern const uint32_t OMRON_VID;
/// Product ID for all omron health devices
extern const uint32_t OMRON_PID;

/*******************************************************************************
 *
 * Omron device structures
 *
 ******************************************************************************/

/**
 * Enumeration for different omron device modes
 *
 * These modes dictate what we're currently trying to do with the
 * omron device. We send a control message with the mode, then
 * do things like get device info (serial number, version, etc...),
 * or engage in specific device communication events, like reading
 * pedometer or blood pressure data
 */
typedef enum
{
	/// Clearing modes and startup/shutdown
	NULL_MODE			= 0x0000,
	/// Getting serial numbers, version, etc...
	DEVICE_INFO_MODE	= 0x1111,
	/// Daily blood pressure info mode
	DAILY_INFO_MODE		= 0x74bc,
	/// Weekly blood pressure info mode
	WEEKLY_INFO_MODE	= 0x1074,
	/// Pedometer info mode
	PEDOMETER_MODE		= 0x0102
} omron_mode;

/**
 * Structure for device state
 *
 * Keeps the state of the device, and the current mode that it's in. If
 * we issue a command that needs a different mode, we can use the state
 * stored here to check that.
 */
typedef struct
{
	/// Device implementation
	omron_device_impl device;
	int input_size;
	int output_size;
	/// Mode the device is currently in
	omron_mode device_mode;
} omron_device;

/*******************************************************************************
 *
 * Blood pressure monitor specific structures
 *
 ******************************************************************************/

/**
 * Structure for daily blood pressure info
 *
 * Stores information taken on a daily basis for blood pressure
 * Usually, we consider there to be one morning and one evening
 * reading.
 */
typedef struct
{
	/// Day of reading
	uint32_t day;
	/// Month of reading
	uint32_t month;
	/// Year of reading
	uint32_t year;
	/// Hour of reading
	uint32_t hour;
	/// Minute of reading
	uint32_t minute;
	/// Second of reading
	uint32_t second;
	uint8_t unit;
	/// SYS reading
	uint32_t sys;
	/// DIA reading
	uint32_t dia;
	/// Pulse reading
	uint32_t pulse;
	/// 1 if week block is filled, 0 otherwise
	uint8_t present;
	uint16_t flags;
} omron_bp_day_info;

/**
 * Structure for weekly blood pressure info
 *
 * Stores information averages for a week
 */
typedef struct
{
	/// Year of reading
	uint32_t year;
	/// Month of reading
	uint32_t month;
	/// Day that weekly average starts on
	uint32_t day;
	/// SYS average for week
	int32_t sys;
	/// DIA average for week
	int32_t dia;
	/// Pulse average for week
	int32_t pulse;
	/// 1 if week block is filled, 0 otherwise
	uint8_t present;
} omron_bp_week_info;


/*******************************************************************************
 *
 * Pedometer specific structures
 *
 ******************************************************************************/

/**
 * Structure for pedometer profile information
 *
 * Stores information about user (weight, stride length, etc...)
 */
typedef struct
{
	/// lbs times 10? i.e. 195 = {0x19, 0x50} off the device
	uint32_t weight;
	/// inches times 10? same as last
	uint32_t stride;
} omron_pd_profile_info;

/**
 * Structure for count of valid pedometer information packets
 *
 * Contains the number of valid daily and hourly packets, for use by
 * programs for reading information off the device
 */
typedef struct
{
	/// Number of valid daily packets
	int32_t daily_count;
	/// Number of valid hourly packets
	int32_t hourly_count;
} omron_pd_count_info;

/**
 * Structure for daily data packets from pedometer
 *
 * Daily information from pedometer, including steps, distance, etc...
 *
 */
typedef struct
{
	/// Total number of steps for the day
	int32_t total_steps;
	/// Total number of "aerobic" steps for the day
	int32_t total_aerobic_steps;
	/// Total time spent "aerobically" walking throughout the day [minutes]
	int32_t total_aerobic_walking_time;
	/// Total calories burned [kcal]
	int32_t total_calories;
	/// Total distance (steps * stride) [miles]
	float total_distance;
	/// Total fat burned [grams]
	float total_fat_burn;
	/// Offset of date from current day
	int32_t day_serial;
} omron_pd_daily_data;

/**
 * Structure for hourly data packets from pedometer
 *
 * Hourly information about steps taken during a certain day
 */
typedef struct
{
	/// Offset of day from current day
	int32_t day_serial;
	/// Index of hour
	int32_t hour_serial;
	/// Was anything happening for the pedometer to record? this also acts as an hour done flag for the current hour; contains 0 even if there is data
	uint8_t is_attached;
	/// Was an event recorded this hour?
	uint8_t event;
	/// Regular steps taken
	int32_t regular_steps;
	/// Aerobic steps taken
	int32_t aerobic_steps;
} omron_pd_hourly_data;


#ifdef __cplusplus
extern "C" {
#endif

	////////////////////////////////////////////////////////////////////////////////////
	//
	// Platform Specific Functions
	//
	////////////////////////////////////////////////////////////////////////////////////
	
	/**
	 * Create a new device structure to work with
	 *
	 * @return Pointer to omron_device structure, or NULL on error
	 */
	OMRON_DECLSPEC omron_device* omron_create();

	/**
	 * Delete and free a device structure created by omron_create()
	 *
	 * @param dev Device pointer
	 */
	OMRON_DECLSPEC void omron_delete(omron_device* dev);

	/**
	 * Returns the number of devices connected, though does not specify device type
	 *
	 * @param dev Device pointer
	 * @param VID Vendor ID, defaults to 0x0590
	 * @param PID Product ID, defaults to 0x0028
	 *
	 * @return Number of devices connected, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_get_count(omron_device* dev, int VID, int PID);

	/**
	 * Open a particular device for communication
	 *
	 * @param dev Device pointer
	 * @param VID Vendor ID, defaults to 0x0590
	 * @param PID Product ID, defaults to 0x0028
	 * @param device_index Index of the device to open
	 *
	 * @return 0 on success, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_open(omron_device* dev, int VID, int PID, uint32_t device_index);

	/**
	 * Closes an open omron device
	 *
	 * @param dev Device pointer to close
	 *
	 * @return 0 on success, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_close(omron_device* dev);

	/**
	 * Sends the control message to set a new mode for the device
	 *
	 * @param dev Device pointer to set mode for
	 * @param mode Mode enumeration value, from omron_mode enum
	 *
	 * @return 0 on success, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_set_mode(omron_device* dev, omron_mode mode);

	/**
	 * Reads data from the device
	 *
	 * @param dev Device pointer to read from
	 * @param report_buf Buffer to read into (should be >= 8 bytes)
	 * @param report_size Size of report_buf (in bytes)
	 * @param timeout Timeout for read operation (in ms)
	 *
	 * @return Number of bytes read, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_read_data(omron_device* dev, uint8_t *report_buf, int report_size, int timeout);

	/**
	 * Writes data to the device
	 *
	 * @param dev Device pointer to write to
	 * @param report_buf Buffer to write data from (should be <= 8 bytes)
	 * @param report_size Size of report_buf (in bytes)
	 * @param timeout Timeout for write operation (in ms)
	 *
	 * @return Number of bytes written, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_write_data(omron_device* dev, uint8_t *report_buf, int report_size, int timeout);

	////////////////////////////////////////////////////////////////////////////////////
	//
	// Device Information Retrieval Functions
	//
	////////////////////////////////////////////////////////////////////////////////////

	/**
	 * Retrieves the serial number of the device.
	 * Returns the result as a zero-terminated string.
	 *
	 * @param dev Device to get serial number from
	 * @param data buffer to read serial number into (must be >= 8 bytes)
	 * @param data_size Size of data buffer (in bytes)
	 *
	 * @return Number of bytes read, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_get_device_serial(omron_device* dev, uint8_t* data, int data_size);

	/**
	 * Retrieves the model and version of the device.
	 * Returns the result as a zero-terminated string.
	 *
	 * @param data buffer to read version into (must be >= 12 bytes)
	 * @param data_size Size of data buffer (in bytes)
	 *
	 * @return Number of bytes read, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_get_device_version(omron_device* dev, uint8_t* data, int data_size);

	////////////////////////////////////////////////////////////////////////////////////
	//
	// Blood Pressure Functions
	//
	////////////////////////////////////////////////////////////////////////////////////

	/**
	 * Get profile information for BP monitor.
	 * Returns the result as a zero-terminated string.
	 *
	 * @param data buffer to read profile into (must be >= 11 bytes)
	 * @param data_size Size of data buffer (in bytes)
	 *
	 * @return Number of bytes read, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_get_bp_profile(omron_device* dev, uint8_t* data, int data_size);

	/**
	 * Get the number of valid daily data packets available.
	 *
	 * @param dev Device to query
	 * @param bank Memory bank to query (A=0, B=1)
	 *
	 * @return Number of available data packets, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_get_daily_data_count(omron_device* dev, unsigned char bank);

	/**
	 * Get daily BP info for a particular bank/day
	 *
	 * @param dev Device to query
	 * @param bank Memory bank to query (A=0, B=1)
	 * @param index Index of day to query in bank
	 *
	 * @return omron_bp_day_info structure with requested information
	 */
	OMRON_DECLSPEC omron_bp_day_info omron_get_daily_bp_data(omron_device* dev, int bank, int index);

	OMRON_DECLSPEC int omron_get_daily_bp_data2(omron_device* dev, int bank, int index, omron_bp_day_info *di);

	/**
	 * Get weekly BP morning or evening info for a particular bank/week
	 *
	 * @param dev Device to query
	 * @param bank Memory bank to query (A=0, B=1)
	 * @param index Index of week to query in bank
	 * @param evening If 0, get morning average, If 1, get evening average.
	 *
	 * @return omron_bp_week_info structure with requested information
	 */
	OMRON_DECLSPEC omron_bp_week_info omron_get_weekly_bp_data(omron_device* dev, int bank, int index, int evening);

	////////////////////////////////////////////////////////////////////////////////////
	//
	// Pedometer Functions
	//
	////////////////////////////////////////////////////////////////////////////////////

	/**
	 * Get pedometer profile information
	 *
	 * @param dev Device to query
	 *
	 * @return omron_pd_profile structure with weight and stride info
	 */
	OMRON_DECLSPEC omron_pd_profile_info omron_get_pd_profile(omron_device* dev);

	/**
	 * Query device for number of valid data packets
	 *
	 * @param dev Device to query
	 *
	 * @return omron_pd_count_info structure with count information
	 */
	OMRON_DECLSPEC omron_pd_count_info omron_get_pd_data_count(omron_device* dev);

	/**
	 * Get daily pedometer averages for a specific day
	 *
	 * @param dev Device to query
	 * @param day Day index (should be between 0 and info retrieved from omron_get_pd_data_count)
	 *
	 * @return omron_pd_daily_data structure
	 */
	OMRON_DECLSPEC omron_pd_daily_data omron_get_pd_daily_data(omron_device* dev, int day);

	/**
	 * Get hourly pedometer data for a specific day
	 *
	 * @param dev Device to query
	 * @param day Day index (should be between 0 and info retrieved from omron_get_pd_data_count)
	 *
	 * @return malloc()d array of 24 omron_pd_hourly_data structures, or NULL on error
	 */
	OMRON_DECLSPEC omron_pd_hourly_data* omron_get_pd_hourly_data(omron_device* dev, int day);

	/**
	 * Clear all readings from the pedometer device
	 *
	 * @param dev Device to clear
	 *
	 * @return 0 on success, or < 0 on error
	 */
	OMRON_DECLSPEC int omron_clear_pd_memory(omron_device* dev);

	////////////////////////////////////////////////////////////////////////////////////
	//
	// Debugging / Errors
	//
	////////////////////////////////////////////////////////////////////////////////////

	/**
	 * Set the debugging level
	 *
	 * @param level Debug level (one of the OMRON_DEBUG_* constants)
	 */
	OMRON_DECLSPEC void omron_set_debug_level(int level);

	/**
	 * Return an error message for a given error code (similar to
	 * strerror() but for errors returned by libomron)
	 *
	 * @param code Error code
	 *
	 * @return Error message string
	 */
	OMRON_DECLSPEC const char *omron_strerror(int code);

#ifdef __cplusplus
}
#endif


#endif //LIBOMRON_H
