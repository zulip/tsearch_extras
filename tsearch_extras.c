/*-------------------------------------------------------------------------
 *
 * This use of this program is subject to the terms of The PostgreSQL License:
 *
 * Copyright (c) 2013, Zulip, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written agreement
 * is hereby granted, provided that the above copyright notice and this
 * paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL ZULIP, INC. BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
 * SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF ZULIP,
 * INC. HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ZULIP, INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND Zulip,
 * Inc. HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/pg_list.h"
#include "tsearch/ts_utils.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_cache.h"
#include "utils/elog.h"
#include "utils/array.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;


typedef struct {
	int32 cur_word;
	int32 num_words;
	int32 char_offset;
	HeadlineWordEntry* words;
} TsMatchesData;

typedef struct {
	int32 offset;
	int32 len;
} TsMatchLocation;

PG_FUNCTION_INFO_V1(ts_match_locs_array);
PG_FUNCTION_INFO_V1(ts_match_locs_array_byid);
PG_FUNCTION_INFO_V1(tsvector_lexemes);

Datum ts_match_locs_array(PG_FUNCTION_ARGS);
Datum ts_match_locs_array_byid(PG_FUNCTION_ARGS);
Datum tsvector_lexemes(PG_FUNCTION_ARGS);

static void
ts_match_locs_setup(Oid cfgId, TsMatchesData *mdata, text* in, TSQuery query)
{
	HeadlineParsedText prs;
	TSConfigCacheEntry *cfg;
	TSParserCacheEntry *prsobj;
	List *headline_options = NIL;

	cfg = lookup_ts_config_cache(cfgId);
	prsobj = lookup_ts_parser_cache(cfg->prsId);

	memset(&prs, 0, sizeof(HeadlineParsedText));
	prs.lenwords = 32;
	prs.words = (HeadlineWordEntry *) palloc(sizeof(HeadlineWordEntry) * prs.lenwords);

	hlparsetext(cfgId, &prs, query, VARDATA(in), VARSIZE(in) - VARHDRSZ);

	#if PG_VERSION_NUM >= 100000
	headline_options = lappend(headline_options,
											  makeDefElem(pstrdup("HighlightAll"),
											  (Node *) makeString(pstrdup("1")), -1));
	#else
	headline_options = lappend(headline_options,
											  makeDefElem(pstrdup("HighlightAll"),
											  (Node *) makeString(pstrdup("1"))));
	#endif

	FunctionCall3(&(prsobj->prsheadline),
				  PointerGetDatum(&prs),
				  PointerGetDatum(headline_options),
				  PointerGetDatum(query));

	mdata->cur_word = 0;
	mdata->char_offset = 0;
	mdata->num_words = prs.curwords;
	mdata->words = prs.words;
}

static bool
ts_match_locs_next_match(TsMatchesData *mdata, TsMatchLocation *match)
{
	while (mdata->cur_word < mdata->num_words)
	{
		HeadlineWordEntry* word = mdata->words + mdata->cur_word;
		int offset = mdata->char_offset;

		mdata->cur_word++;
		if (! word->skip && ! word->repeated)
		{
			mdata->char_offset += word->len;

			if (word->selected)
			{
				match->offset = offset;
				match->len = word->len;
				return true;
			}
		}
	}

	return false;
}

Datum
ts_match_locs_array_byid(PG_FUNCTION_ARGS)
{
	TsMatchesData mdata;
	TsMatchLocation match;
	Oid cfgId = PG_GETARG_OID(0);
	text *in = PG_GETARG_TEXT_P(1);
	TSQuery query = PG_GETARG_TSQUERY(2);
	ArrayType *result;
	Datum *elems;
	int num_matches_allocd = 6; /* a random guess */
	int num_matches = 0;
	int result_dims[2];
	int result_lbs[2];

	elems = palloc(sizeof(Datum) * 2 * num_matches_allocd);

	ts_match_locs_setup(cfgId, &mdata, in, query);

	while (ts_match_locs_next_match(&mdata, &match))
	{
		if (num_matches >= num_matches_allocd) {
			num_matches_allocd *= 1.5;
			elems = repalloc(elems, sizeof(Datum) * 2 * num_matches_allocd);
		}
		elems[num_matches * 2] = Int32GetDatum(match.offset);
		elems[num_matches * 2 + 1] = Int32GetDatum(match.len);

		++num_matches;
	}

	result_dims[0] = num_matches;
	result_dims[1] = 2;
	result_lbs[0] = 1;
	result_lbs[1] = 1;
	result = construct_md_array(elems, NULL, 2, result_dims, result_lbs, INT4OID,
								sizeof(int32), true, 'i');
	pfree(elems);

	PG_RETURN_POINTER(result);
}

Datum
ts_match_locs_array(PG_FUNCTION_ARGS)
{
	Oid cfgId;
	text *in = PG_GETARG_TEXT_P(0);
	TSQuery query = PG_GETARG_TSQUERY(1);

	cfgId = getTSCurrentConfig(true);
	PG_RETURN_DATUM(DirectFunctionCall3(ts_match_locs_array_byid,
										ObjectIdGetDatum(cfgId),
										PointerGetDatum(in),
										TSQueryGetDatum(query)));
}

Datum
tsvector_lexemes(PG_FUNCTION_ARGS)
{
	TSVector tsvec = PG_GETARG_TSVECTOR(0);
	Datum *elems;
	ArrayType *ret;
	int i;

	elems = palloc(sizeof(Datum) * tsvec->size);
	for (i = 0; i < tsvec->size; ++i)
	{
		WordEntry entry = tsvec->entries[i];
		char *lexeme = STRPTR(tsvec) + entry.pos;
		elems[i] = PointerGetDatum(cstring_to_text_with_len(lexeme, entry.len));
	}

	ret = construct_array(elems, tsvec->size, TEXTOID, -1, false, 'i');

	for (i = 0; i < tsvec->size; ++i)
	{
		pfree(DatumGetTextP(elems[i]));
	}
	pfree(elems);
	PG_RETURN_POINTER(ret);
}
