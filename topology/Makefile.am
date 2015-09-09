bin_PROGRAMS = \
	alsatplg

alsatplg_SOURCES = topology.c

AM_CPPFLAGS = \
         -Wall -I$(top_srcdir)/include

alsatplg_LDADD = -lasound

# local build
AM_CPPFLAGS += -I$(top_srcdir)/../alsa-lib/include
topology_LDADD = -L$(top_srcdir)/../alsa-lib/src/.libs
