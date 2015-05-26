/*
  Copyright(c) 2014-2015 Intel Corporation
  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "topology.h"

static void usage(char *name)
{
	fprintf(stdout, "usage: %s config outfile [options]\n\n", name);

	fprintf(stdout, "Add vendor firmware text	[-vfw firmware]\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	snd_tplg_t *snd_tplg;
	
	if (argc < 3)
		usage(argv[0]);

	snd_tplg = snd_tplg_new();
	if (snd_tplg == NULL) {
		fprintf(stderr, "failed to open %s\n", argv[argc - 1]);
		exit(0);
	}

	snd_tplg_build(snd_tplg, argv[1], argv[2]);

	snd_tplg_free(snd_tplg);
	return 0;
}

