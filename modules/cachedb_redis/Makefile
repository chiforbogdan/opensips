# $Id: Makefile 6253 2009-10-12 19:00:53Z bogdan_iancu $
#
# Presence Agent
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile

include ../../Makefile.defs
auto_gen=
NAME=cachedb_redis.so

DEFS+=-I$(LOCALBASE)/include
LIBS=-L$(LOCALBASE)/lib -lhiredis

include ../../Makefile.modules
