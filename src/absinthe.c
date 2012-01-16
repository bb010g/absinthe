/**
 * GreenPois0n Absinthe - absinthe.c
 * Copyright (C) 2010 Chronic-Dev Team
 * Copyright (C) 2010 Joshua Hill
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>

#include <sys/signal.h>
#include <plist/plist.h>

#include <libimobiledevice/sbservices.h>

#include "mb1.h"
#include "rop.h"
#include "debug.h"
#include "backup.h"
#include "device.h"
#include "boolean.h"
#include "jailbreak.h"
#include "dictionary.h"
#include "crashreporter.h"
#include "idevicebackup2.h"
#include "idevicepair.h"

#include "dyldcache.h"

#include "offsets.h"

/////////////////////////////////////////////////////////////////////////////////////////
/// TODO: We need to add an event handler for when devices are connected. This handler //
///         needs to wait for iTunes to autostart and kill it before it can start the  //
///         syncing process and mess up our connection.                                //
/////////////////////////////////////////////////////////////////////////////////////////

static struct option longopts[] = {
	{ "help",        no_argument,         NULL,   'h' },
	{ "verbose",     required_argument,   NULL,   'v' },
	{ "uuid",        required_argument,   NULL,   'u' },
	{ "target",      required_argument,   NULL,   't' },
	{ "pointer",     required_argument,   NULL,   'p' },
	{ "aslr-slide",  required_argument,   NULL,   'a' },
	{ NULL, 0, NULL, 0 }
};

unsigned long find_aslr_slide(crashreport_t* crash, char* cache) {
	unsigned long slide = 0;
	if(crash == NULL || cache == NULL) {
		error("Invalid arguments\n");
		return 0;
	}

	dyldcache_t* dyldcache = dyldcache_open(cache);
	if(dyldcache != NULL) {
		dyldcache_free(dyldcache);
	}
	return slide;
}

static void idevice_event_cb(const idevice_event_t *event, void *user_data)
{
	/*char* uuid = (char*)user_data;
	printf("device event %d: %s\n", event->event, event->uuid);
	if (uuid && strcmp(uuid, event->uuid)) return;
	if (event->event == IDEVICE_DEVICE_ADD) {
		connected = 1;
	} else if (event->event == IDEVICE_DEVICE_REMOVE) {
		connected = 0;
	}*/
	jb_device_event_cb(event, user_data);
}

static void signal_handler(int sig)
{
	jb_signal_handler(sig);
}

void usage(int argc, char* argv[]) {
	char* name = strrchr(argv[0], '/');
	printf("Usage: %s [OPTIONS]\n", (name ? name + 1 : argv[0]));
	printf("(c) 2011-2012, Chronic-Dev LLC\n");
	printf("Jailbreak iOS5.0 using ub3rl33t MobileBackup2 exploit.\n");
	//printf("Discovered by Nikias Bassen, Exploited by Joshua Hill\n");
	printf("  General\n");
	printf("    -h, --help\t\t\tprints usage information\n");
	printf("    -v, --verbose\t\tprints debuging info while running\n");
	printf("    -u, --uuid UUID\t\ttarget specific device by its 40-digit device UUID\n");
	printf("\n  Payload Generation\n");
	printf("    -t, --target ADDRESS\toffset to ROP gadget we want to execute\n");
	printf("    -p, --pointer ADDRESS\theap address we're hoping contains our target\n");
	printf("    -a, --aslr-slide OFFSET\tvalue of randomized dyldcache slide\n");
	printf("\n");
}

const char* lastmsg = NULL;
static void status_cb(const char* msg, int progress)
{
	if (!msg) {
		msg = lastmsg;
	} else {
		lastmsg = msg;
	}
	printf("[%d%%] %s\n", progress,msg);
}

int main(int argc, char* argv[]) {
	device_t* device = NULL;

	int opt = 0;
	int optindex = 0;

	int verbose = 0;
	unsigned long aslr_slide = 0;
	unsigned long pointer = 0;
	unsigned long target = 0;
	char* uuid = NULL;

	while ((opt = getopt_long(argc, argv, "hva:p:t:u:", longopts, &optindex)) > 0) {
		switch (opt) {
		case 'h':
			usage(argc, argv);
			return 0;

		case 'v':
			verbose++;
			break;

		case 'a':
			aslr_slide = strtoul(optarg, NULL, 0);
			break;

		case 'p':
			pointer = strtoul(optarg, NULL, 0);
			break;

		case 't':
			target = strtoul(optarg, NULL, 0);
			break;

		case 'u':
			uuid = strdup(optarg);
			break;

		default:
			usage(argc, argv);
			return -1;
		}
	}

	if ((argc-optind) == 0) {
		argc -= optind;
		argv += optind;

	} else {
		usage(argc, argv);
		return -1;
	}

	/* we need to exit cleanly on running backups and restores or we cause havok */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
#ifndef WIN32	
	signal(SIGQUIT, signal_handler);
	signal(SIGPIPE, SIG_IGN);
#endif

	if (!uuid) {
		device = device_create(NULL);
		if (!device) {
			error("No device found, is it plugged in?\n");
			return -1;
		}
		uuid = strdup(device->uuid);
		device_free(device);
		device = NULL;
	}

	idevice_event_subscribe(idevice_event_cb, uuid);

	jailbreak(uuid, status_cb);

	idevice_event_unsubscribe();

	free(uuid);

	return 0;
}
