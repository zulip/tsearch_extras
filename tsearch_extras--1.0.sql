-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION tsearch_extras" to load this file. \quit

CREATE FUNCTION ts_match_locs(IN regconfig, IN text, IN tsquery, OUT int4, OUT int4)
	RETURNS SETOF RECORD
        as 'MODULE_PATHNAME', 'ts_match_locs_byid'
        LANGUAGE C STRICT;

CREATE FUNCTION ts_match_locs(IN text, IN tsquery, OUT int4, OUT int4)
	RETURNS SETOF RECORD
        as 'MODULE_PATHNAME', 'ts_match_locs'
        LANGUAGE C STRICT;

CREATE FUNCTION ts_match_locs_array(regconfig, text, tsquery)
	RETURNS int4[][2]
        as 'MODULE_PATHNAME', 'ts_match_locs_array_byid'
        LANGUAGE C STRICT;

CREATE FUNCTION ts_match_locs_array(text, tsquery)
	RETURNS int4[][2]
        as 'MODULE_PATHNAME', 'ts_match_locs_array'
        LANGUAGE C STRICT;

CREATE FUNCTION tsvector_lexemes(tsvector)
	RETURNS text[]
        as 'MODULE_PATHNAME', 'tsvector_lexemes'
        LANGUAGE C STRICT;
