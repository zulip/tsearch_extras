MODULES = zulip_tsearch
EXTENSION = zulip_tsearch
DATA = zulip_tsearch--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
