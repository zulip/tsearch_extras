-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION zulip_tsearch" to load this file. \quit

CREATE FUNCTION ts_match_locs(regconfig, text, tsquery)
	RETURNS SETOF RECORD
        as 'MODULE_PATHNAME', 'ts_match_locs_byid'
        LANGUAGE C STRICT;

CREATE FUNCTION ts_match_locs(text, tsquery)
	RETURNS SETOF RECORD
        as 'MODULE_PATHNAME', 'ts_match_locs'
        LANGUAGE C STRICT;

CREATE FUNCTION ts_match_locs_array(regconfig, text, tsquery)
	RETURNS integer[][2]
        as 'MODULE_PATHNAME', 'ts_match_locs_array_byid'
        LANGUAGE C STRICT;

CREATE FUNCTION ts_match_locs_array(text, tsquery)
	RETURNS integer[][2]
        as 'MODULE_PATHNAME', 'ts_match_locs_array'
        LANGUAGE C STRICT;
