# contrib/jsonknife/Makefile

MODULE_big = jsonknife
OBJS = jsonknife.o

EXTENSION = jsonknife
DATA = jsonknife--1.0.sql

REGRESS = test
# We need a UTF8 database
ENCODING = UTF8

ifdef USE_PGXS
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/jsonknife
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
