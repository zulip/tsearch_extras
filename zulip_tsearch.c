#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "tsearch/ts_utils.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_cache.h"
#include "utils/elog.h"

PG_MODULE_MAGIC;


typedef struct {
	int4 cur_word;
	int4 num_words;
	int4 char_offset;
	HeadlineWordEntry* words;
} TsMatchesData;

typedef struct {
	int4 offset;
	int4 len;
} TsMatchLocation;

PG_FUNCTION_INFO_V1(ts_match_locs);
Datum ts_match_locs(PG_FUNCTION_ARGS);

static void
ts_match_locs_setup(TsMatchesData *mdata, text* in, TSQuery query)
{
	Oid cfgId;
	HeadlineParsedText prs;
	TSConfigCacheEntry *cfg;
	TSParserCacheEntry *prsobj;

	cfgId = getTSCurrentConfig(true);
	cfg = lookup_ts_config_cache(cfgId);
	prsobj = lookup_ts_parser_cache(cfg->prsId);

	memset(&prs, 0, sizeof(HeadlineParsedText));
	prs.lenwords = 32;
	prs.words = (HeadlineWordEntry *) palloc(sizeof(HeadlineWordEntry) * prs.lenwords);

	hlparsetext(cfgId, &prs, query, VARDATA(in), VARSIZE(in) - VARHDRSZ);

	FunctionCall3(&(prsobj->prsheadline),
				  PointerGetDatum(&prs),
				  PointerGetDatum(NIL),
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

		mdata->char_offset += word->len;
		mdata->cur_word++;

		if (word->selected)
		{
			match->offset = offset;
			match->len = word->len;
			return true;
		}
	}

	return false;
}

Datum
ts_match_locs(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	TupleDesc tupdesc;
	TsMatchesData *mdata;
	TsMatchLocation match;
	MemoryContext oldcontext;

	if (SRF_IS_FIRSTCALL())
    {
		text *in = PG_GETARG_TEXT_P(0);
		TSQuery query = PG_GETARG_TSQUERY(1);

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		mdata = (TsMatchesData *) palloc(sizeof(TsMatchesData));
		ts_match_locs_setup(mdata, in, query);
		funcctx->user_fctx = mdata;

		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context "
                            "that cannot accept type record")));
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

        MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();

	mdata = (TsMatchesData *) funcctx->user_fctx;
	if (ts_match_locs_next_match(mdata, &match)) {
		HeapTuple return_tuple;
		Datum *values = (Datum *) palloc(sizeof(Datum) * 2);
		bool *nulls = (bool *) palloc(sizeof(bool) * 2);
		nulls[0] = false;
		nulls[1] = false;
		values[0] = Int32GetDatum(match.offset);
		values[1] = Int32GetDatum(match.len);
		return_tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(return_tuple));
	}

	SRF_RETURN_DONE(funcctx);
}
