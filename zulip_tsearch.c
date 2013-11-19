#include "postgres.h"

#include "fmgr.h"
#include "tsearch/ts_utils.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_cache.h"
#include "utils/elog.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(ts_matches);
Datum ts_matches(PG_FUNCTION_ARGS);

Datum
ts_matches(PG_FUNCTION_ARGS)
{
	text *in = PG_GETARG_TEXT_P(0);
	TSQuery query = PG_GETARG_TSQUERY(1);
	Oid cfgId;
	HeadlineParsedText prs;
	int i;
	int global_pos = 0;
	TSConfigCacheEntry *cfg;
	TSParserCacheEntry *prsobj;

	cfgId = getTSCurrentConfig(true);
	cfg = lookup_ts_config_cache(cfgId);
	prsobj = lookup_ts_parser_cache(cfg->prsId);

	prs.lenwords = (VARSIZE(in) - VARHDRSZ) / 6;		/* just estimation of
														 * word's number */
	memset(&prs, 0, sizeof(HeadlineParsedText));
	prs.lenwords = 32;
	prs.words = (HeadlineWordEntry *) palloc(sizeof(HeadlineWordEntry) * prs.lenwords);

	hlparsetext(cfgId, &prs, query, VARDATA(in), VARSIZE(in) - VARHDRSZ);

	FunctionCall3(&(prsobj->prsheadline),
				  PointerGetDatum(&prs),
				  PointerGetDatum(NIL),
				  PointerGetDatum(query));

	for (i = 0; i < prs.curwords; ++i)
	{
		HeadlineWordEntry* word = &prs.words[i];
		if (word->selected)
		{
		ereport(NOTICE,
				(errcode(ERRCODE_SUCCESSFUL_COMPLETION),
				 errmsg("'%s' %d %d", word->word, global_pos, word->len)));
		}
		global_pos += word->len;
	}

	pfree(prs.words);

	PG_RETURN_INT32(5);
}
