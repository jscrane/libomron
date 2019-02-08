//TODO: check OMRON_ERR_DEVIO instances to see if any of them should be BADDATA instead
/*
 * Generic function file for Omron Health User Space Driver
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

#include "libomron/omron.h"
#include "omron_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Global constants declared in omron.h
const uint32_t OMRON_VID = 0x0590;
const uint32_t OMRON_PID = 0x0028;

// NOTE: The following list must be kept consistent with the error constants
//       defined in omron.h!
static const char *err_msgs[] = {
	"No error (rc >= 0)",				// [noerr] (0)
	"USB communication error",			// DEVIO   (-1)
	"Bad argument",					// BADARG  (-2)
	"Device not open",				// NOTOPEN (-3)
	"Supplied buffer is the wrong size",		// BUFSIZE (-4)
	"Bad or unrecognized command",			// NEGRESP (-5)
	"Unexpected END response received",		// ENDRESP (-6)
	"Device returned bad data",			// BADDATA (-7)
	"Unknown error",				// <= -8
};

OMRON_DECLSPEC const char *omron_strerror(int code) {
	const int num_err_msgs = sizeof(err_msgs) / sizeof(err_msgs[0]);

	if (code >= 0) {
		return err_msgs[0];
	}
	code = -code;
	if (code >= num_err_msgs) {
		code = num_err_msgs - 1;
	}
	return err_msgs[code];
}

int _omron_debug_level = -1; // This will get set initially from omron_create()

OMRON_DECLSPEC void omron_set_debug_level(int level) {
	char *env_setting;
	char *endptr;
	int new_level;

	if (level < 0) {
		if (_omron_debug_level < 0) {
			level = DEBUG_DEFAULT_LEVEL;
		} else {
			return;
		}
	}
	new_level = level;

	env_setting = getenv(DEBUG_ENV_VAR);
	if (env_setting) {
		// Any setting in the environment will override anything the
		// program sets.
		new_level = strtol(env_setting, &endptr, 0);
		if (*endptr || (endptr == env_setting)) {
			// Invalid chars in value.  Ignore env setting.
			DPRINTF(_omron_debug_level, "Invalid value for %s environment variable ('%s').  Ignoring.\n", DEBUG_ENV_VAR, env_setting);
			new_level = level;
		}
	}
	_omron_debug_level = new_level;
}

void omron_hexdump(const uint8_t *data, int n_bytes)
{
	while (n_bytes--) {
		fprintf(stderr, " %02x", *(unsigned char*) data);
		data++;
	}
	fprintf(stderr, "\n");
}

int bcd_to_int(unsigned char *data, int start, int length)
{
	int ret = 0;
	int pos = start;
	int i;
	for(i = 0; i < length; ++i)
	{
		if(!(i % 2))
		{
			if(i >= 2)
				pos++;
			ret += (data[pos] >> 4) * (pow(10, length - i - 1));
		}
		else
		{
			ret += (data[pos] & 0x0f) * (pow(10, length - i - 1));
		}
	}
	return ret;
}

/*
* For data not starting on byte boundaries
*/
int bcd_to_int2(unsigned char *data, int start_nibble, int len_nibbles)
{
	int ret = 0;
	int nib, abs_nib, i;
	for(i=0; i < len_nibbles; i++) {
		abs_nib = start_nibble + i;
		nib = (data[abs_nib / 2] >> (4 * (1 - abs_nib % 2))) & 0x0F;
		ret += nib * pow(10, len_nibbles - i - 1);
	}
	return ret;
}

short short_to_bcd(int number)
{
	return ((number/10) << 4) | (number % 10);
}


int omron_send_command(omron_device* dev, int size, const unsigned char* buf)
{
	int total_write_size = 0;
	int current_write_size;
	unsigned char output_report[dev->output_size];
	int status;

	if (buf[0] == 0) {
		MSG_INFO("Sending clear command...\n");
	} else {
		MSG_INFO("Sending %c%c%c command...\n", buf[0], buf[1], buf[2]);
	}
	MSG_HEXDUMP(OMRON_DEBUG_PROTO, "Command: ", buf, size);

	while(total_write_size < size)
	{
		current_write_size = size - total_write_size;
		if(current_write_size >= dev->output_size)
			current_write_size = dev->output_size - 1;

		output_report[0] = current_write_size;
		memcpy(output_report + 1, buf+total_write_size,
		       current_write_size);

		status = omron_write_data(dev, output_report, sizeof(output_report), 1000);
		if (status < 0) return status;
		total_write_size += current_write_size;
	}

	return 0;
}

int omron_check_success(unsigned char *input_report)
{
	if (input_report[0] == 'O' && input_report[1] == 'K') {
		//FIXME: we should check the checksum here too
		return 0;
	} else if (input_report[0] == 'N' && input_report[1] == 'O') {
		MSG_DETAIL("Response = 'NO'\n");
		return OMRON_ERR_NEGRESP;
	}
	MSG_DETAIL("Response is not 'OK' or 'NO' (bad data?)\n");
	return OMRON_ERR_BADDATA;
}

/* Read and discard any queued-up input so we're (hopefully) in sync with the
 * device on the other end again.
 */
static int omron_flush(omron_device *dev) {
	int status;
	unsigned char input_report[dev->input_size];
	int flushed = 0;

	MSG_DETAIL("Flushing any extra input...\n");
	while (1) {
		/* We use a shorter-than-usual timeout (0.1s) because we know we're going to hit it eventually (so we want to keep it short) and we're only really concerned about queued input (which should be immediately retrievable) */
		/* Note: The negative value for 'timeout' means it's "expected" and should not generate an error (just a 0 result) */
		status = omron_read_data(dev, input_report, sizeof(input_report), -100);
		if (status < 0) return status;
		if (status == 0) {
			/* Timeout (this is expected sooner or later) */
			break;
		}
		flushed += status;
		MSG_DETAIL("Discarding %d bytes of extra data...\n", status);
	};
	MSG_DETAIL("Flush complete.\n");
	return flushed;
}

static int
xor_checksum(unsigned char *data, int len)
{
	unsigned char checksum = 0;

	while (len--)
		checksum ^= *(data++);

	if (checksum)
		MSG_DETAIL("bad checksum: 0x%x != 0\n", checksum);

	return checksum;
}

/*
  omron_get_command_return returns:
  >=0 : Valid response ("OK..."). Returns # of bytes (including "OK").
  OMRON_ERR_NEGRESP: "NO" response
  OMRON_ERR_BADDATA: Garbled response
  <0 : Other error
*/

int omron_get_command_return(omron_device* dev, int size, unsigned char* data)
{
	int total_read_size = 0;
	int current_read_size = 0;
	unsigned char input_report[dev->input_size];
	int read_result;
	const int max_data_chunk = sizeof(input_report) - 1;

	do {
		read_result = omron_read_data(dev, input_report, sizeof(input_report), 1000);
		if (read_result < 0) return read_result;

		current_read_size = input_report[0];
		if (current_read_size < 0) {
			MSG_ERROR("Invalid size byte: %d\n", current_read_size);
			return OMRON_ERR_DEVIO;
		} else if (current_read_size > sizeof(input_report)) {
			MSG_ERROR("Invalid size byte: %d\n", current_read_size);
			return OMRON_ERR_DEVIO;
		} else if (current_read_size > max_data_chunk) {
			MSG_WARN("(size byte == report size.  Adjusting to report size - 1)\n");
			current_read_size = max_data_chunk; /* FIXME? Bug? */
		}

		/* printf(" current_read=%d size=%d total_read_size=%d.\n", */
		/* 		current_read_size, size, total_read_size); */

		if (current_read_size > size - total_read_size) {
			// This shouldn't happen.  Just ignore any extra we got
			// back..
			MSG_WARN("Received more data than expected (%d > %d).  Ignoring extra.", total_read_size + current_read_size, size);
			current_read_size = size - total_read_size;
		}

		memcpy(data + total_read_size, input_report + 1,
		       current_read_size);
		total_read_size += current_read_size;

		if (current_read_size < max_data_chunk) {
			// Short chunks should only occur as the last chunk of
			// a response.  Stop here (even if we haven't read as
			// much as we were expecting) because trying to read
			// further will just result in a timeout.
			break;
		}
		if (data[0] != 'O' || data[1] != 'K') {
			// Only "OK" responses have the potential to span more
			// than one report.  We either have a "NO", an "END",
			// or a garbled response.  In any case, we should stop
			// here.
			break;
		}
	} while (total_read_size < size);
	MSG_HEXDUMP(OMRON_DEBUG_PROTO, "Response: ", data, total_read_size);

	if (total_read_size < 2) {
		MSG_ERROR("Response is too short: bad data.\n");
		return OMRON_ERR_BADDATA;
	}
	if (data[0] == 'O' && data[1] == 'K') {
		if (xor_checksum(data+2, total_read_size-2)) {
			MSG_ERROR("Response has bad checksum: bad data.\n");
			return OMRON_ERR_BADDATA;
		}
		MSG_DETAIL("Received OK response: requested=%d read=%d\n", size, total_read_size);
		return total_read_size;
	}
	if (data[0] == 'N' && data[1] == 'O') {
		if (total_read_size != 2) {
			MSG_WARN("'NO' response has extra data.\n");
		}
		MSG_DETAIL("Received NO response.\n");
		return OMRON_ERR_NEGRESP;
	}
	if (!strncmp((const char*) data, "END\r\n", total_read_size)) {
		MSG_DETAIL("Received END response.\n");
		return OMRON_ERR_ENDRESP;
	}
	MSG_ERROR("Response is not OK, NO, or END: bad data.\n");
	return OMRON_ERR_BADDATA;
}

int omron_send_clear(omron_device* dev)
{
	static const unsigned char zero[12]; /* = all zeroes */
	unsigned char response[2];
	int status;
	int retry_count = 3;

	MSG_INFO("Performing clear...\n")
	do {
		status = omron_flush(dev);
		if (status < 0) break;
		status = omron_send_command(dev, sizeof(zero), zero);
		if (status < 0) break;
		status = omron_get_command_return(dev, sizeof(response), response);
		if (status != OMRON_ERR_BADDATA) break;
	} while (retry_count--);

	if (status < 0) {
		MSG_ERROR("Clear failed: %d\n", status);
		return status;
	}
	MSG_INFO("Clear successful.\n")
	return 0;
}

int omron_check_mode(omron_device* dev, omron_mode mode)
{
	int ret;

	if(dev->device_mode == mode)
		return 0;

	ret = omron_set_mode(dev, mode);
	if(ret == 0)
	{
		dev->device_mode = mode;
		return omron_send_clear(dev);
	}
	return ret;
}

static int omron_exchange_cmd(omron_device *dev,
			       omron_mode mode,
			       int cmd_len,
			       const unsigned char *cmd,
			       int response_len,
			       unsigned char *response)
{
	int status;
	
	status = omron_check_mode(dev, mode);
	if (status < 0) return status;

	status = omron_send_command(dev, cmd_len, cmd);
	if (status < 0) return status;
	status = omron_get_command_return(dev, response_len, response);
	if (status != OMRON_ERR_BADDATA) return status;

	// Got a garbled response.  Do a flush and try again.
	MSG_WARN("Bad response from device.  Retrying...\n");
	status = omron_flush(dev);
	if (status < 0) return status;
	status = omron_send_command(dev, cmd_len, cmd);
	if (status < 0) return status;
	status = omron_get_command_return(dev, response_len, response);
	if (status != OMRON_ERR_BADDATA) return status;

	// Hmm.. still garbled.  Try doing a full clear/resync and try again.
	MSG_WARN("Bad response from device.  Resyncing and retrying...\n");
	status = omron_set_mode(dev, mode);
	if (status < 0) return status;
	status = omron_send_clear(dev);
	if (status < 0) return status;
	status = omron_send_command(dev, cmd_len, cmd);
	if (status < 0) return status;
	status = omron_get_command_return(dev, response_len, response);
	if (status != OMRON_ERR_BADDATA) return status;

	// Ok, we still can't get a valid response.  Time to just give up.
	MSG_ERROR("Unable to get a valid response from device.\n");
	return status;
}

static int
omron_dev_info_command(omron_device* dev,
		       const char *cmd,
		       unsigned char *result,
		       int result_max_len)
{
	unsigned char tmp[result_max_len+3];
	int status;

	status = omron_exchange_cmd(dev, PEDOMETER_MODE, strlen(cmd),
			   (const unsigned char*) cmd,
			   result_max_len+3, tmp);
	if (status < 0) return status;
        if (status < 3) return OMRON_ERR_DEVIO;
	memcpy(result, tmp + 3, status - 3);
	return status - 3;
}

//platform independant functions
OMRON_DECLSPEC omron_device* omron_create()
{
	omron_set_debug_level(-1); // Initialize to default if not already set
	MSG_INFO("Creating new device.\n");
	return omron_create_device();
}

OMRON_DECLSPEC int omron_get_device_version(omron_device* dev, unsigned char* data, int data_size)
{
	int status;
	const int response_size = 12;

	if (data_size <= response_size) {
		MSG_ERROR("Supplied buffer too small (%d < %d)\n", data_size, response_size + 1);
		return OMRON_ERR_BUFSIZE;
	}
	status = omron_dev_info_command(dev, "VER00", data, response_size);
	if (status < 0) return status;
	data[status] = 0;
	return status;
}

OMRON_DECLSPEC int omron_get_bp_profile(omron_device* dev, unsigned char* data, int data_size)
{
	int status;
	const int response_size = 11;

	if (data_size <= response_size) {
		MSG_ERROR("Supplied buffer too small (%d < %d)\n", data_size, response_size + 1);
		return OMRON_ERR_BUFSIZE;
	}
	status = omron_dev_info_command(dev, "PRF00", data, response_size);
	if (status < 0) return status;
	data[status] = 0;
	return status;
}

OMRON_DECLSPEC int omron_get_device_serial(omron_device* dev, unsigned char* data, int data_size)
{
	int status;
	const int response_size = 8;

	if (data_size <= response_size) {
		MSG_ERROR("Supplied buffer too small (%d < %d)\n", data_size, response_size + 1);
		return OMRON_ERR_BUFSIZE;
	}
	status = omron_dev_info_command(dev, "SRL00", data, response_size);
	if (status < 0) return status;
	data[status] = 0;
	return status;
}

//daily data information
OMRON_DECLSPEC int omron_get_daily_data_count(omron_device* dev, unsigned char bank)
{
	unsigned char data[8];
	unsigned char command[8] =
		{ 'G', 'D', 'C', 0x00, bank, 0x00, 0x00, bank };
	int status;

	status = omron_exchange_cmd(dev, DAILY_INFO_MODE, sizeof(command), command,
			   sizeof(data), data);
	if (status < 0) {
		return status;
	} else if (status != sizeof(data)) {
		MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
		return OMRON_ERR_BADDATA;
	}
	MSG_INFO("Data units found: %d\n", (int)data[6]);
	return (int)data[6];
}

// HEM-637
OMRON_DECLSPEC int omron_get_daily_bp_data2(omron_device* dev, int bank, int index, omron_bp_day_info *r)
{
	unsigned char data[15];
	unsigned char command[7] = { 'M', 'E', 'S', 0x00, 0x00,
					index, index ^ bank};
	int status;

	status = omron_exchange_cmd(dev, DAILY_INFO_MODE, sizeof(command), command,
			   sizeof(data), data);

	r->present = 0;
	if (status != sizeof(data)) {
		MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
		return status;
	}
	status = omron_check_success(data);
	if (status) {
		MSG_ERROR("Request failed.\n");
		return status;
	}
	r->present = 1;
	r->year = data[3];
	r->month = data[4];
	r->day = data[5];
	r->hour = data[6];
	r->minute = data[7];
	r->second = data[8];
	r->unit = data[9];
	r->sys = (data[10] << 8) + data[11];
	r->dia = data[12];
	r->pulse = data[13];
	r->flags = 0;
	return 0;
}

OMRON_DECLSPEC omron_bp_day_info omron_get_daily_bp_data(omron_device* dev, int bank, int index)
{
	omron_bp_day_info r;
	unsigned char data[17];
	unsigned char command[8] = {'G', 'M', 'E', 0x00, bank, 0x00,
				    index, index ^ bank};
	int status;

	memset(&r, 0 , sizeof(r));
	memset(data, 0, sizeof(data));

	status = omron_exchange_cmd(dev, DAILY_INFO_MODE, sizeof(command), command,
			   sizeof(data), data);

	if (status != sizeof(data)) {
		MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
		//FIXME: We need a way to return an error result from this function
		r.present = 0;
	} else if (omron_check_success(data)) {
		MSG_ERROR("Request failed.\n");
		r.present = 0;
	} else {
		r.present = 1;
		r.year = data[3];
		r.month = data[4];
		r.day = data[5];
		r.hour = data[6];
		r.minute = data[7];
		r.second = data[8];
		r.unit = data[9];
		r.sys = (data[10] << 8) + data[11];
		r.dia = data[12];
		r.pulse = data[13];
		r.flags = (data[14] << 8) + data[15];
	}
	return r;
}

OMRON_DECLSPEC omron_bp_week_info omron_get_weekly_bp_data(omron_device* dev, int bank, int index, int evening)
{
	omron_bp_week_info r;
	unsigned char data[12];	/* 12? */
	unsigned char command[9] = { 'G',
				     (evening ? 'E' : 'M'),
				     'A',
				     0,
				     bank,
				     index,
				     0,
				     0,
				     index^bank };
	int status;

	memset(&r, 0 , sizeof(r));
	memset(data, 0, sizeof(data));

	status = omron_exchange_cmd(dev, WEEKLY_INFO_MODE, sizeof(command), command,
			   sizeof(data), data);

	if (status != sizeof(data)) {
		MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
		//FIXME: We need a way to return an error result from this function
		r.present = 0;
	} else if (omron_check_success(data)) {
		MSG_ERROR("Request failed.\n");
		r.present = 0;
	} else {
		r.present = 1;

		// Unknown: 3 (always 0x00)
		// Unknown: 4 (always 0x80)
		r.year = data[5];
		r.month = data[6];
		r.day = data[7];
		r.sys = data[8] + 25;
		r.dia = data[9];
		r.pulse = data[10];
		// Unknown: 11
	}
	return r;
}

OMRON_DECLSPEC omron_pd_profile_info omron_get_pd_profile(omron_device* dev)
{
	unsigned char data[11];
	omron_pd_profile_info profile_info;
	int status;

	status = omron_dev_info_command(dev, "PRF00", data, sizeof(data));
	if (status == sizeof(data)) {
		// Unknown: 0..1
		profile_info.weight = bcd_to_int(data, 2, 4) / 10;
		// Unknown: 4..5
		profile_info.stride = bcd_to_int(data, 6, 4) / 10;
		// Unknown: 8..10
	} else {
		MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
		//FIXME: We need a way to return an error result from this function
		memset(&profile_info, 0, sizeof(profile_info));
	}
	return profile_info;
}

OMRON_DECLSPEC int omron_clear_pd_memory(omron_device* dev)
{
	unsigned char data[2];
	int status;

	status = omron_exchange_cmd(dev, PEDOMETER_MODE, 5, (unsigned char*)"CTD00", 2, data);
	if (status < 0) return status;
	if (status != 2) return OMRON_ERR_BADDATA;
	return omron_check_success(data);
}

OMRON_DECLSPEC omron_pd_count_info omron_get_pd_data_count(omron_device* dev)
{
	omron_pd_count_info count_info;
	unsigned char data[5];
	int status;

	status = omron_dev_info_command(dev, "CNT00", data, sizeof(data));
	if (status == sizeof(data)) {
		// Unknown: 0
		count_info.daily_count = data[1];
		// Unknown: 2
		count_info.hourly_count = data[3];
		// Unknown: 4
	} else {
		MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
		//FIXME: We need a way to return an error result from this function
		memset(&count_info, 0, sizeof(count_info));
	}
	return count_info;
}

OMRON_DECLSPEC omron_pd_daily_data omron_get_pd_daily_data(omron_device* dev, int day)
{
	omron_pd_daily_data daily_data;
	unsigned char data[20];
	unsigned char command[7] =
		{ 'M', 'E', 'S', 0x00, 0x00, day, 0x00 ^ day};
	int status;

	status = omron_exchange_cmd(dev, PEDOMETER_MODE, sizeof(command), command,
			   sizeof(data), data);
	if (status == sizeof(data)) {
		daily_data.total_steps = bcd_to_int2(data, 6, 5);
		daily_data.total_aerobic_steps = bcd_to_int2(data, 11, 5);
		daily_data.total_aerobic_walking_time = bcd_to_int2(data, 16, 4);
		daily_data.total_calories = bcd_to_int2(data, 20, 5);
		daily_data.total_distance = bcd_to_int2(data, 25, 5) / 100.0;
		daily_data.total_fat_burn = bcd_to_int2(data, 30, 4) / 10.0;
		// Unknown: 17..19
		daily_data.day_serial = day;
	} else {
		MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
		//FIXME: We need a way to return an error result from this function
		memset(&daily_data, 0, sizeof(daily_data));
	}
	return daily_data;
}

OMRON_DECLSPEC omron_pd_hourly_data* omron_get_pd_hourly_data(omron_device* dev, int day)
{
	omron_pd_hourly_data* hourly_data = malloc(sizeof(omron_pd_hourly_data) * 24);
	unsigned char data[37];
	int status;

	if (!hourly_data) {
		return NULL;
	}
	int i, j;
	for(i = 0; i < 3; ++i)
	{
		unsigned char command[8] =
			{ 'G', 'T', 'D', 0x00, 0, day, i + 1, day ^ (i + 1)};
		status = omron_exchange_cmd(dev, PEDOMETER_MODE, sizeof(command), command,
						   sizeof(data), data);
		if (status != sizeof(data)) {
			MSG_ERROR("Returned data size (%d) does not match expected size (%lu)!\n", status, sizeof(data));
			//FIXME: would be better if we had a way to return an
			//       actual error code instead of just NULL
			free(hourly_data);
			return NULL;
		}
		for(j = 0; j <= 7; ++j)
		{
			int offset = j * 4 + 4;
			int hour = (i * 8) + j;
			hourly_data[hour].is_attached = (data[(offset)] & (1 << 6)) > 0;
			hourly_data[hour].regular_steps = ((data[(offset)] & (~0xc0)) << 8) | (data[(offset) + 1]);
			hourly_data[hour].event = (data[(offset) + 2] & (1 << 6)) > 0;
			hourly_data[hour].aerobic_steps = ((data[(offset) + 2] & (~0xc0)) << 8) | (data[(offset) + 3]);
			hourly_data[hour].hour_serial = hour;
			hourly_data[hour].day_serial = day;
		}
	}
	return hourly_data;
}
