#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "tsearch/ts_utils.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_cache.h"
#include "utils/elog.h"

PG_MODULE_MAGIC;


typedef struct {
	int4 pos;
	int4 len;
	int4 global_pos;
	HeadlineWordEntry* words;
} TsMatchesData;

PG_FUNCTION_INFO_V1(ts_matches);
Datum ts_matches(PG_FUNCTION_ARGS);

static void
ts_matches_setup(FuncCallContext *funcctx, text* in, TSQuery query)
{
	Oid cfgId;
	HeadlineParsedText prs;
	TSConfigCacheEntry *cfg;
	TSParserCacheEntry *prsobj;
	TsMatchesData *mdata;

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

	mdata = (TsMatchesData *) palloc(sizeof(TsMatchesData));
	mdata->pos = 0;
	mdata->global_pos = 0;
	mdata->len = prs.curwords;
	mdata->words = prs.words;

	funcctx->user_fctx = (void *) mdata;
}

Datum
ts_matches(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	TupleDesc tupdesc;
	TsMatchesData *mdata;
	MemoryContext oldcontext;

	if (SRF_IS_FIRSTCALL())
    {
		text *in = PG_GETARG_TEXT_P(0);
		TSQuery query = PG_GETARG_TSQUERY(1);

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		ts_matches_setup(funcctx, in, query);

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

	while (mdata->pos < mdata->len)
	{
		HeadlineWordEntry* word = mdata->words + mdata->pos;
		HeapTuple return_tuple;
		int current_pos = mdata->global_pos;
		char wordbuf[30];

		mdata->global_pos += word->len;
		mdata->pos++;

		memcpy(wordbuf, word->word, word->len);
		wordbuf[word->len] = 0;
		ereport(NOTICE,
				(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
				 errmsg("'%s' %d %d", wordbuf, current_pos, word->len)));

		if (word->selected)
		{
			Datum *values = (Datum *) palloc(sizeof(Datum) * 2);
			bool *nulls = (bool *) palloc(sizeof(bool) * 2);
			nulls[0] = false;
			nulls[1] = false;
			values[0] = Int32GetDatum(current_pos);
			values[1] = Int32GetDatum(word->len);
			return_tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
			ereport(NOTICE,
					(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
					 errmsg("selected word: %s", wordbuf)));

			SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(return_tuple));
		}
	}

	SRF_RETURN_DONE(funcctx);
}
