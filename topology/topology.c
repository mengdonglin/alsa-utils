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
#include <getopt.h>
#include <assert.h>

//#include <alsa/asoundlib.h>

#include <local.h>
#include <sound/asound.h>
#include <sound/tlv.h>
#include <sound/asoc.h>

#include <alsa/topology.h>
#include "gettext.h"

static snd_output_t *log;

static void usage(char *name)
{
	printf(
_("Usage: %s [OPTIONS]...\n"
"\n"
"-h, --help              help\n"
"-c, --compile=FILE      compile file\n"
"-v, --verbose=LEVEL     set verbosity level (0...1)\n"
"-o, --output=FILE       set output file\n"
"-t, --test=ENABLE       enable test mode (1 enable, 0 disable)\n"
), name);
}

static int init_template_hdr(struct snd_tplg_ctl_template *hdr,
	int type, const char *name, int access,
	struct snd_tplg_io_ops_template *ops,
	struct snd_tplg_tlv_template *tlv)
{
	hdr->type = type;
	hdr->name = name;
	hdr->access = access;

	if (ops != NULL)
		hdr->ops = *ops;

	hdr->tlv = tlv;
	return 0;
}

static struct snd_tplg_channel_map_template* create_master_playback_volume_channel(void)
{
	struct snd_tplg_channel_map_template *map;
	struct snd_tplg_channel_elem *elem;

	if ((map = malloc(sizeof(struct snd_tplg_channel_map_template))) == NULL)
		return NULL;

	map->num_channels = 2;

	elem = &map->channel[0];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 0;
	elem->shift = 0;
	elem->id = SNDRV_CHMAP_FL;

	elem = &map->channel[1];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 0;
	elem->shift = 8;
	elem->id = SNDRV_CHMAP_FR;

	return map;
}

/*
SectionTLV."hsw_vol_tlv" {
	Comment "TLV used by both global and stream volumes"

	scale {
		min "-9000"
		step "300"
		mute "1"
	}
}

SectionControlMixer."Master Playback Volume" {
	Comment "Global DSP volume"

	# control belongs to this index group
	index "1"

	# Channel register and shift for Front Left/Right
	channel."FL" {
		reg "0"
		shift "0"
	}
	channel."FR" {
		reg "0"
		shift "8"
	}

	# max control value and whether value is inverted
	max "31"
	invert "false"

	# control uses bespoke driver get/put/info ID 0
	ops."ctl" {
		info "volsw"
		get "256"
		put "256"
	}

	# uses TLV data above
	tlv "hsw_vol_tlv"
}
*/
static int add_master_playback_volume(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_mixer_template m;
	struct snd_tplg_io_ops_template ops;
	struct snd_tplg_tlv_dbscale_template tlv;

	memset(&m, 0, sizeof(struct snd_tplg_mixer_template));

	ops.get = 256;
	ops.put = 256;
	ops.info = SND_SOC_TPLG_CTL_VOLSW;

	tlv.hdr.type = SNDRV_CTL_TLVT_DB_SCALE;
	tlv.min = -9000;
	tlv.step = 300;
	tlv.mute = 1;

	if (init_template_hdr(&m.hdr, SND_SOC_TPLG_TYPE_MIXER,
		"Master Playback Volume",
		SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
		&ops, &tlv.hdr) != 0)
		return -1;

	if ((m.map = create_master_playback_volume_channel()) == NULL)
		return -1;

	m.min = 0;
	m.max = 31;
	m.invert = 0;
	//m.platform_max = 31;	/* jinyao: correct? */

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_MIXER;
	t.mixer = &m;

	return snd_tplg_add_object(tplg, &t);
}

static struct snd_tplg_channel_map_template* create_media0_playback_volume_channel(void)
{
	struct snd_tplg_channel_map_template *map;
	struct snd_tplg_channel_elem *elem;

	if ((map = malloc(sizeof(struct snd_tplg_channel_map_template))) == NULL)
		return NULL;

	map->num_channels = 2;

	elem = &map->channel[0];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 1;
	elem->shift = 0;
	elem->id = SNDRV_CHMAP_FL;

	elem = &map->channel[1];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 1;
	elem->shift = 8;
	elem->id = SNDRV_CHMAP_FR;

	return map;
}

/*
SectionControlMixer."Media0 Playback Volume" {
	Comment "Offload 0 volume"

	# control belongs to this index group
	index "1"

	# Channel register and shift for Front Left/Right
	channel."FL" {
		reg "1"
		shift "0"
	}
	channel."FR" {
		reg "1"
		shift "8"
	}

	# max control value and whether value is inverted
	max "31"
	invert "false"

	# control uses bespoke driver get/put/info ID 0
	ops."ctl" {
		info "volsw"
		get "257"
		put "257"
	}

	# uses TLV data above
	tlv "hsw_vol_tlv"
}
*/
static int add_media0_playback_volume(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_mixer_template m;
	struct snd_tplg_io_ops_template ops;
	struct snd_tplg_tlv_dbscale_template tlv;

	memset(&m, 0, sizeof(struct snd_tplg_mixer_template));

	ops.get = 257;
	ops.put = 257;
	ops.info = SND_SOC_TPLG_CTL_VOLSW;

	tlv.hdr.type = SNDRV_CTL_TLVT_DB_SCALE;
	tlv.min = -9000;
	tlv.step = 300;
	tlv.mute = 1;

	if (init_template_hdr(&m.hdr, SND_SOC_TPLG_TYPE_MIXER,
		"Media0 Playback Volume",
		SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
		&ops, &tlv.hdr) != 0)
		return -1;

	if ((m.map = create_media0_playback_volume_channel()) == NULL)
		return -1;

	m.min = 0;
	m.max = 31;
	m.invert = 0;
	//m.platform_max = 31;	/* jinyao: correct? */

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_MIXER;
	t.mixer = &m;

	return snd_tplg_add_object(tplg, &t);
}

static struct snd_tplg_channel_map_template* create_media1_playback_volume_channel(void)
{
	struct snd_tplg_channel_map_template *map;
	struct snd_tplg_channel_elem *elem;

	if ((map = malloc(sizeof(struct snd_tplg_channel_map_template))) == NULL)
		return NULL;

	map->num_channels = 2;

	elem = &map->channel[0];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 2;
	elem->shift = 0;
	elem->id = SNDRV_CHMAP_FL;

	elem = &map->channel[1];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 2;
	elem->shift = 8;
	elem->id = SNDRV_CHMAP_FR;

	return map;
}

/*
SectionControlMixer."Media1 Playback Volume" {
	Comment "Offload 1 volume"

	# control belongs to this index group
	index "1"

	# Channel register and shift for Front Left/Right
	channel."FL" {
		reg "2"
		shift "0"
	}
	channel."FR" {
		reg "2"
		shift "8"
	}

	# max control value and whether value is inverted
	max "31"
	invert "false"

	# control uses bespoke driver get/put/info ID 0
	ops."ctl" {
		info "volsw"
		get "257"
		put "257"
	}

	# uses TLV data above
	tlv "hsw_vol_tlv"
}
*/
static int add_media1_playback_volume(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_mixer_template m;
	struct snd_tplg_io_ops_template ops;
	struct snd_tplg_tlv_dbscale_template tlv;

	memset(&m, 0, sizeof(struct snd_tplg_mixer_template));

	ops.get = 257;
	ops.put = 257;
	ops.info = SND_SOC_TPLG_CTL_VOLSW;

	tlv.hdr.type = SNDRV_CTL_TLVT_DB_SCALE;
	tlv.min = -9000;
	tlv.step = 300;
	tlv.mute = 1;

	if (init_template_hdr(&m.hdr, SND_SOC_TPLG_TYPE_MIXER,
		"Media1 Playback Volume",
		SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
		&ops, &tlv.hdr) != 0)
		return -1;

	if ((m.map = create_media1_playback_volume_channel()) == NULL)
		return -1;

	m.min = 0;
	m.max = 31;
	m.invert = 0;
	//m.platform_max = 31;	/* jinyao: correct? */

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_MIXER;
	t.mixer = &m;

	return snd_tplg_add_object(tplg, &t);
}

static struct snd_tplg_channel_map_template* create_mic_capture_volume_channel(void)
{
	struct snd_tplg_channel_map_template *map;
	struct snd_tplg_channel_elem *elem;

	if ((map = malloc(sizeof(struct snd_tplg_channel_map_template))) == NULL)
		return NULL;

	map->num_channels = 2;

	elem = &map->channel[0];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 0;
	elem->shift = 0;
	elem->id = SNDRV_CHMAP_FL;

	elem = &map->channel[1];
	elem->size = sizeof(struct snd_tplg_channel_elem);
	elem->reg = 0;
	elem->shift = 8;
	elem->id = SNDRV_CHMAP_FR;

	return map;
}

/*
SectionControlMixer."Mic Capture Volume" {
	Comment "Mic Capture volume"

	# control belongs to this index group
	index "1"

	# Channel register and shift for Front Left/Right
	channel."FL" {
		reg "0"
		shift "0"
	}
	channel."FR" {
		reg "0"
		shift "8"
	}

	# max control value and whether value is inverted
	max "31"
	invert "false"

	# control uses bespoke driver get/put/info ID 0
	ops."ctl" {
		info "volsw"
		get "257"
		put "257"
	}

	# uses TLV data above
	tlv "hsw_vol_tlv"
}
*/
static int add_mic_capture_volume(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_mixer_template m;
	struct snd_tplg_io_ops_template ops;
	struct snd_tplg_tlv_dbscale_template tlv;

	memset(&m, 0, sizeof(struct snd_tplg_mixer_template));

	ops.get = 257;
	ops.put = 257;
	ops.info = SND_SOC_TPLG_CTL_VOLSW;

	tlv.hdr.type = SNDRV_CTL_TLVT_DB_SCALE;
	tlv.min = -9000;
	tlv.step = 300;
	tlv.mute = 1;

	if (init_template_hdr(&m.hdr, SND_SOC_TPLG_TYPE_MIXER,
		"Mic Capture Volume",
		SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE,
		&ops, &tlv.hdr) != 0)
		return -1;

	if ((m.map = create_mic_capture_volume_channel()) == NULL)
		return -1;

	m.min = 0;
	m.max = 31;
	m.invert = 0;
	//m.platform_max = 31;	/* jinyao: correct? */

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_MIXER;
	t.mixer = &m;

	return snd_tplg_add_object(tplg, &t);
}

/*
SectionWidget."SSP0 CODEC IN" {

	index "1"
	type "aif_in"
	no_pm "true"
	shift "0"
	invert "0"
}
*/
static int add_ssp0_codec_in(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_widget_template w;

	memset(&w, 0, sizeof(w));
	w.id = SND_SOC_TPLG_DAPM_AIF_IN;
	w.name = "SSP0 CODEC IN";
	w.reg  = -1;

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_DAPM_WIDGET;
	t.index = 1;
	t.widget = &w;

	return snd_tplg_add_object(tplg, &t);
}

/*
SectionWidget."SSP0 CODEC OUT" {

	index "1"
	type "aif_out"
	no_pm "true"
	shift "0"
	invert "0"
}
*/
static int add_ssp0_codec_out(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_widget_template w;

	memset(&w, 0, sizeof(w));
	w.id = SND_SOC_TPLG_DAPM_AIF_OUT;
	w.name = "SSP0 CODEC OUT";
	w.reg  = -1;

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_DAPM_WIDGET;
	t.index = 1;
	t.widget = &w;

	return snd_tplg_add_object(tplg, &t);
}

/*
SectionWidget."SSP1 BT IN" {

	index "1"
	type "aif_in"
	no_pm "true"
	shift "0"
	invert "0"
}
*/
static int add_ssp1_bt_in(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_widget_template w;

	memset(&w, 0, sizeof(w));
	w.id = SND_SOC_TPLG_DAPM_AIF_IN;
	w.name = "SSP1 BT IN";
	w.reg  = -1;

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_DAPM_WIDGET;
	t.index = 1;
	t.widget = &w;

	return snd_tplg_add_object(tplg, &t);
}

/*
SectionWidget."SSP1 BT OUT" {

	index "1"
	type "aif_out"
	no_pm "true"
	shift "0"
	invert "0"
}
*/
static int add_ssp1_bt_out(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_widget_template w;

	memset(&w, 0, sizeof(w));
	w.id = SND_SOC_TPLG_DAPM_AIF_OUT;
	w.name = "SSP1 BT OUT";
	w.reg  = -1;

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_DAPM_WIDGET;
	t.index = 1;
	t.widget = &w;

	return snd_tplg_add_object(tplg, &t);
}

/*
SectionWidget."Playback VMixer" {

	index "1"
	type "mixer"
	no_pm "true"
	shift "0"
	invert "0"
}
*/
static int add_playback_vmixer(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_widget_template w;

	memset(&w, 0, sizeof(w));
	w.id = SND_SOC_TPLG_DAPM_MIXER;
	w.name = "Playback VMixer";
	w.reg  = -1;

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_DAPM_WIDGET;
	t.index = 1;
	t.widget = &w;

	return snd_tplg_add_object(tplg, &t);
}

/*
SectionGraph."dsp" {
	index "1"

	lines [
		"Playback VMixer, , System Playback"
		"Playback VMixer, , Offload0 Playback"
		"Playback VMixer, , Offload1 Playback"
		"SSP0 CODEC OUT, , Playback VMixer"
		"Loopback Capture, , Playback VMixer"
		"Analog Capture, , SSP0 CODEC IN"
	]
}
*/
static int add_graph(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	struct snd_tplg_graph_template* g;
	int size, num_routes = 5;
	struct snd_tplg_graph_elem elems[] = {
		{"System Playback", NULL, "Playback VMixer"},
		{"Offload0 Playback", NULL, "Playback VMixer"},
		{"Offload1 Playback", NULL, "Playback VMixer"},
		{"Playback VMixer", NULL, "SSP0 CODEC OUT"},
		{"SSP0 CODEC IN", NULL, "Analog Capture"},
	};

	size = sizeof(struct snd_tplg_graph_template) +
		num_routes * sizeof(struct snd_tplg_graph_elem);

	if ((g = malloc(size)) == NULL)
		return -1;

	g->count = num_routes;
	memcpy(g->elem, elems, sizeof(struct snd_tplg_graph_elem) * g->count);

	memset(&t, 0, sizeof(t));
	t.type = SND_TPLG_TYPE_DAPM_GRAPH;
	t.graph = g;

	return snd_tplg_add_object(tplg, &t);
}

struct snd_tplg_stream_caps_template bdw_pcm_caps_sys_play  = {
	.name = "System Playback",
	.formats = (1 << SNDRV_PCM_FORMAT_S16_LE) | (1 << SNDRV_PCM_FORMAT_S24_LE),
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

struct snd_tplg_stream_caps_template bdw_pcm_caps_anlaog_cap  = {
	.name = "Analog Capture",
	.formats = (1 << SNDRV_PCM_FORMAT_S16_LE) | (1 << SNDRV_PCM_FORMAT_S24_LE),
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 4,
};

struct snd_tplg_stream_caps_template bdw_pcm_caps_loopbk_cap  = {
	.name = "Loopback Capture",
	.formats = (1 << SNDRV_PCM_FORMAT_S16_LE) | (1 << SNDRV_PCM_FORMAT_S24_LE),
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};


struct snd_tplg_stream_caps_template bdw_pcm_caps_off0_play  = {
	.name = "Offload0 Playback",
	.formats = (1 << SNDRV_PCM_FORMAT_S16_LE) | (1 << SNDRV_PCM_FORMAT_S24_LE),
	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 2,
};

struct snd_tplg_stream_caps_template bdw_pcm_caps_off1_play  = {
	.name = "Offload1 Playback",
	.formats = (1 << SNDRV_PCM_FORMAT_S16_LE) | (1 << SNDRV_PCM_FORMAT_S24_LE),
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static char priv_data[] = {0x08, 0x07,0x06,0x05,0x04, 0x03,0x02,0x01};
struct snd_soc_tplg_private *test_priv = NULL;


static struct snd_tplg_pcm_template bdw_pcms[] = {
	{
		.pcm_name = "System Playback/Capture",
		.dai_name =  "System Pin",
		.pcm_id = 0,
		.dai_id = 0,
		.playback = 1,
		.capture = 1,
		.caps[0] = &bdw_pcm_caps_sys_play,
		.caps[1] = &bdw_pcm_caps_anlaog_cap,
	},
	{
		.pcm_name = "Offload0 Playback",
		.dai_name =  "Offload0 Pin",
		.pcm_id = 0,
		.dai_id = 1,
		.playback = 1,
		.caps[0] = &bdw_pcm_caps_off0_play,
	},
	{
		.pcm_name = "Offload1 Playback",
		.dai_name =  "Offload1 Pin",
		.pcm_id = 0,
		.dai_id = 2,
		.playback = 1,
		.caps[0] = &bdw_pcm_caps_off1_play,
	},
	{
		.pcm_name = "Loopback",
		.dai_name =  "Loopback Pin",
		.pcm_id = 0,
		.dai_id = 3,
		.capture = 1,
		.caps[1] = &bdw_pcm_caps_loopbk_cap,
	},
};


static struct snd_tplg_link_cmpnt_template be_link_cpu = {
	.dai_name = "snd-soc-dummy-dai",
};

static struct snd_tplg_link_cmpnt_template be_link_codec = {
	.name = "i2c-INT343A:00",
	.dai_name = "rt286-aif1",
};

static struct snd_tplg_hw_config_template be_link_hw_config = {
	.fmt = SND_SOC_DAI_FORMAT_I2S,
	.bclk_master = 1,
	.fsync_master =1,
};

static struct snd_tplg_link_template bdw_be_link = {
		.id = 8, /* for test only */
		.name = "Codec",
		.cpu = &be_link_cpu,
		.codecs = &be_link_codec,
		.num_codecs = 1,
		.hw_config = &be_link_hw_config,
		.num_hw_configs = 1,
		.flag_mask = SND_SOC_TPLG_LNK_FLGBIT_IGNORE_SUSPEND
				| SND_SOC_TPLG_LNK_FLGBIT_IGNORE_POWERDOWN_TIME
				| SND_SOC_TPLG_LNK_FLGBIT_DPCM_PLAYBACK
				| SND_SOC_TPLG_LNK_FLGBIT_DPCM_CAPTURE,
		.flags = SND_SOC_TPLG_LNK_FLGBIT_IGNORE_SUSPEND
				| SND_SOC_TPLG_LNK_FLGBIT_IGNORE_POWERDOWN_TIME
				| SND_SOC_TPLG_LNK_FLGBIT_DPCM_PLAYBACK
				| SND_SOC_TPLG_LNK_FLGBIT_DPCM_CAPTURE,
};

static struct snd_tplg_stream_caps_template ssp0_playback  = {
	.name = "ssp0 Tx",
	.formats = 1 << SNDRV_PCM_FORMAT_S16_LE ,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static struct snd_tplg_stream_caps_template ssp0_capture  = {
	.name = "ssp0 Rx",
	.formats = 1 << SNDRV_PCM_FORMAT_S16_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static struct snd_tplg_be_dai_template be = {
	.dai_id = 1,
	.dai_name = "SSP0 Pin",
	.playback = 1,
	.capture = 1,
	.caps[0] = &ssp0_playback,
	.caps[1] = &ssp0_capture,
};

static int add_be_links(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	int ret;

	t.type = SND_TPLG_TYPE_BE;
	t.link = &bdw_be_link;

	ret = snd_tplg_add_object(tplg, &t);

	if (ret) {
		fprintf(stderr, "add BE link %s failed\n", t.link->name);
		return ret;
	}

	fprintf(stderr, "add BE link %s succesfully\n", t.link->name);

	return 0;
}

static int add_be_dai(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	int ret;

	/* Just for test. BDW does not explicity define a BE DAI */
	t.type = SND_TPLG_TYPE_BE_DAI;
	t.be_dai= &be;

	ret = snd_tplg_add_object(tplg, &t);
	if (ret) {
		fprintf(stderr, "add BE DAI %s failed\n", be.dai_name);
		return ret;
	}

	fprintf(stderr, "add BE link %s succesfully\n", be.dai_name);
	return 0;
}

static int add_pcms(snd_tplg_t *tplg)
{
	snd_tplg_obj_template_t t;
	int ret, i;
	struct snd_soc_tplg_private *priv = NULL;

	priv = calloc(sizeof(*priv)+8, 1);
	if (!priv)
		return -1;
	priv->size = 4;
	memcpy(priv->data, priv_data, 4);

	for (i = 0; i < 4 ; i++) {
		t.type = SND_TPLG_TYPE_PCM;
		t.pcm = &bdw_pcms[i];
		t.pcm->priv = priv;

		ret = snd_tplg_add_object(tplg, &t) ;
		if (ret) {
			fprintf(stderr, "add pcm %s failed\n", t.pcm->pcm_name);
			return ret;
		}

		fprintf(stderr, "add pcm %s succesfully\n", t.pcm->pcm_name);

	}

	free(priv);
	return 0;
}

int test_c_api_for_bdw(snd_tplg_t *tplg, const char *outfile)
{
	test_priv = calloc(sizeof(*test_priv)+8, 1);
	if (!test_priv)
		return -1;
	test_priv->size = 4;
	memcpy(test_priv->data, priv_data, 4);

	if (add_master_playback_volume(tplg) != 0) {
		fprintf(stderr, "add mixer 'Master Playback Volume' failed\n");
		return -1;
	}

	if (add_media0_playback_volume(tplg) != 0) {
		fprintf(stderr, "add mixer 'Media0 Playback Volume' failed\n");
		return -1;
	}

	if (add_media1_playback_volume(tplg) != 0) {
		fprintf(stderr, "add mixer 'Media1 Playback Volume' failed\n");
		return -1;
	}

	if (add_mic_capture_volume(tplg) != 0) {
		fprintf(stderr, "add mixer 'Mic Capture Volume' failed\n");
		return -1;
	}

	if (add_ssp0_codec_in(tplg) != 0) {
		fprintf(stderr, "add widget 'SSP0 CODEC IN' failed\n");
		return -1;
	}

	if (add_ssp0_codec_out(tplg) != 0) {
		fprintf(stderr, "add widget 'SSP0 CODEC OUT' failed\n");
		return -1;
	}

	if (add_ssp1_bt_in(tplg) != 0) {
		fprintf(stderr, "add widget 'SSP1 BT IN' failed\n");
		return -1;
	}

	if (add_ssp1_bt_out(tplg) != 0) {
		fprintf(stderr, "add widget 'SSP1 BT OUT' failed\n");
		return -1;
	}

	if (add_playback_vmixer(tplg) != 0) {
		fprintf(stderr, "add widget 'Playback VMixer' failed\n");
		return -1;
	}

	if (add_graph(tplg) != 0) {
		fprintf(stderr, "add graph 'dsp' failed\n");
		return -1;
	}

	if (add_pcms(tplg) != 0) {
		fprintf(stderr, "add PCMs failed\n");
		return -1;
	}

	if (add_be_links(tplg) != 0) {
		fprintf(stderr, "add BE links failed\n");
		return -1;
	}

	if (add_be_dai(tplg) != 0) {
		fprintf(stderr, "add BE DAI failed\n");
		return -1;
	}

	free(test_priv);
	return snd_tplg_build(tplg, outfile);
}

int main(int argc, char *argv[])
{
	snd_tplg_t *snd_tplg;
	static const char short_options[] = "hc:v:o:t:";
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"verbose", 0, 0, 'v'},
		{"compile", 0, 0, 'c'},
		{"output", 0, 0, 'o'},
		{"test", 0, 0, 't'},
		{0, 0, 0, 0},
	};
	char *source_file = NULL, *output_file = NULL;
	int c, err, verbose = 0, test_mode = 0, option_index;

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	textdomain(PACKAGE);
#endif

	err = snd_output_stdio_attach(&log, stderr, 0);
	assert(err >= 0);

	while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			usage(argv[0]);
			return 0;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 'c':
			source_file = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;

		case 't':
			test_mode = atoi(optarg);
			break;
			
		default:
			fprintf(stderr, _("Try `%s --help' for more information.\n"), argv[0]);
			return 1;
		}
	}

	if ((source_file == NULL && !test_mode)|| output_file == NULL) {
		usage(argv[0]);
		return 1;
	}

	snd_tplg = snd_tplg_new();
	if (snd_tplg == NULL) {
		fprintf(stderr, _("failed to create new topology context\n"));
		return 1;
	}

	snd_tplg_verbose(snd_tplg, verbose);

	if (test_mode) {
		err = test_c_api_for_bdw(snd_tplg, output_file);
	}
	else {
		err = snd_tplg_build_file(snd_tplg, source_file, output_file);
	}
	if (err < 0) {
		fprintf(stderr, _("failed to compile context %s\n"), source_file);
		snd_tplg_free(snd_tplg);
		return 1;
	}

	snd_tplg_free(snd_tplg);
	return 0;
}

