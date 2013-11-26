MODULES = tsearch_extras
EXTENSION = tsearch_extras
DATA = tsearch_extras--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
