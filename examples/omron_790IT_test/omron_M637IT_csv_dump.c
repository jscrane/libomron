#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libomron/omron.h>

int main(int argc, char *argv[])
{
	int opt, bank = 0;
	omron_device *dev = omron_create();
	int verbose = 0;

	while ((opt = getopt(argc, argv, "vd:")) != -1) {
		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case 'd':
			omron_set_debug_level(atoi(optarg));
			break;
		default:
			fprintf(stderr, "Usage: %s [-v] [-d n] [bank]\n", argv[0]);
			return 1;
		}
	}

	if (optind < argc)
		bank = atoi(argv[optind]);

	int n = omron_get_count(dev, OMRON_VID, OMRON_PID);
	if (n < 0) {
		fprintf(stderr, "Device scan failed: %s\n", omron_strerror(n));
		return n;
	}
	if (n == 0) {
		fprintf(stderr, "No devices found\n");
		return 1;
	}
	if (verbose)
		printf("Found %d devices\n", n);

	// open first device
	int ret = omron_open(dev, OMRON_VID, OMRON_PID, 0);
	if (ret < 0) {
		fprintf(stderr, "Device open failed: %s\n", omron_strerror(ret));
		return ret;
	}
	if (verbose)
		printf("Device is in mode %04x\n", dev->device_mode);

	char buf[32];
	ret = omron_get_device_version(dev, buf, sizeof(buf));
	if (ret < 0) {
		fprintf(stderr, "Failed to get device version: %s\n", omron_strerror(ret));
		goto close;
	}
	if (verbose)
		printf("Device version: %s\n", buf);

	ret = omron_get_bp_profile(dev, buf, sizeof(buf));
	if (ret < 0) {
		fprintf(stderr, "Get BP profile failed: %s\n", omron_strerror(ret));
		goto close;
	}
	if (verbose)
		printf("BP profile: %s\n", buf);

	ret = omron_get_device_serial(dev, buf, sizeof(buf));
	if (ret < 0) {
		fprintf(stderr, "Get Serial failed: %s\n", omron_strerror(ret));
		goto close;
	}

	omron_pd_count_info ci = omron_get_pd_data_count(dev);

	printf("date,time,sys,dia,pulse\n");
	for (int i = 0; i < ci.daily_count; i++) {
		omron_bp_day_info di;
		ret = omron_get_daily_bp_data2(dev, bank, i, &di);
		if (ret < 0) {
			fprintf(stderr, "Get daily bp data failed: (%d) %s\n", i, omron_strerror(n));
			goto close;
		}
		printf("20%02d-%02d-%02d,%02d:%02d:%02d,%d,%d,%d\n",
			di.year, di.month, di.day, di.hour, di.minute, 
			di.second, di.sys, di.dia, di.pulse);
	}
 
	ret = omron_end(dev, buf, sizeof(buf));
	if (ret < 0) {
		fprintf(stderr, "End failed: %s\n", omron_strerror(ret));
		goto close;
	}
	if (verbose)
		printf("End: %s\n", buf);
		
close:
	ret = omron_close(dev);
	if (ret < 0) {
		fprintf(stderr, "Device close failed: %s\n", omron_strerror(ret));
		return ret;
	}
	return ret;
}
