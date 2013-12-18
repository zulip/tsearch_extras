tsearch_extras
==============

The package provides a few PostgreSQL functions that allow you to get at
lower-level data about full text search.

The currently available functions are:

| Function | Description |
| -------- | ----------- |
| ts_match_locs_array(regconfig, text, tsquery) | Returns the locations and lengths of matches as an array of 2-element arrays |
| ts_match_locs_array(text, tsquery) | Same as above, but with the current text search configuration |
| tsvector_lexemes(tsvector) | Returns an array of the lexemes in the given tsvector |

Examples
========

~~~
pgdb=> SELECT ts_match_locs_array('The quick brown fox jumped over the lazy dog.  The jumping was quite fast.',
                                  plainto_tsquery('jump'));
 ts_match_locs_array
---------------------
 {{20,6},{51,7}}
(1 row)
~~~

~~~
pgdb=> SELECT tsvector_lexemes(to_tsvector('The quick brown fox jumped over the lazy dog.  The jumping was quite fast.'));
             tsvector_lexemes
-------------------------------------------
 {brown,dog,fast,fox,jump,lazi,quick,quit}
(1 row)
~~~
