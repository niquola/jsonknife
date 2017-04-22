create extension jsonknife;

SELECT knife_extract('{"a":{"b": {"c": 5.3}}}', '[["a","b","c"]]'); 

SELECT knife_extract('{"a":[{"b": [{"c": 5.3}, {"c": 6.3}]}, {"b": [{"c": 5.3}, {"c": 6.3}]}]}', '[["a","b","c"]]'); 

SELECT knife_extract('{"a": [1, 2, {"b": {"c": 5.3}}, {"b": {"c": 100}}]}', '[["a", 2 ,"b", "c"]]'); 

SELECT knife_extract('{"name": [{"use": "official", "given": ["a", "b"]}, {"use": "common", "given": ["c", "d"]}]}', '[["name",{"use": "official"},"given"]]');
SELECT knife_extract('{"name": [{"use": "official", "given": ["a", "b"]}, {"use": "official", "given": ["c", "d"]}]}', '[["name",{"use": "official"},"given"]]'); 

SELECT knife_extract('{"name": [{"a": {"b": 1}, "c": "ok"}, {"a": {"b": 2}, "c": "fail"}]}', '[["name", {"a":{"b":1}}, "c"]]');

SELECT knife_extract('{"a":{"b": {"c": 5.3, "d": 6.7}}}', '[["a","b","c"],["a","b","d"]]'); 

SELECT knife_extract_text(
  '{"a": {"b": [{"c": "l", "d": "o"}, {"c": 1, "d": ["b", "o", "k"]}]}}',
  '[["a","b","c"],["a","b","d"]]'
  );


SELECT
'^' ||
array_to_string(
  knife_extract_text(
    '{"a": {"b": [{"c": "l", "d": "o"}, {"c": 1, "d": ["b", "o", "k"]}]}}',
    '[["a","b","c"],["a","b","d"]]'
  ),
  '$^'
)
|| '$'
;

SELECT knife_extract_numeric(
'{"a": 1, "b": [2,3], "c": {"d": 5}}',
'[["a"],["b"],["c", "d"]]'
);

SELECT knife_extract_max_numeric(
'{"a": 1, "b": 2, "c": {"d": 5}}',
'[["a"],["b"],["c", "d"]]'
);

SELECT knife_extract_min_numeric(
'{"a": 1, "b": 2, "c": {"d": 5}}',
'[["a"],["b"],["c", "d"]]'
);

SELECT knife_extract_min_timestamptz('{"a":{"b": {"c": "1980"}}}', '[["a", "b", "c"]]');
SELECT knife_extract_min_timestamptz('{"a":{"b": {"c": "1980-02"}}}', '[["a","b","c"]]');
SELECT knife_extract_min_timestamptz('{"a":{"b": {"c": "1980-02-05"}}}', '[["a","b","c"]]');
SELECT knife_extract_min_timestamptz('{"a":{"b": {"c": "1980-02-05T08"}}}', '[["a","b","c"]]');
SELECT knife_extract_min_timestamptz('{"a":{"b": {"c": "1980-02-05T08:30"}}}', '[["a","b","c"]]');

SELECT knife_extract_max_timestamptz('{"a":{"b": {"c": "1980"}}}', '[["a", "b", "c"]]');
SELECT knife_extract_max_timestamptz('{"a":{"b": {"c": "1980-02"}}}', '[["a","b","c"]]');
SELECT knife_extract_max_timestamptz('{"a":{"b": {"c": "1980-02-05"}}}', '[["a","b","c"]]');
SELECT knife_extract_max_timestamptz('{"a":{"b": {"c": "1980-02-05T08"}}}', '[["a","b","c"]]');
SELECT knife_extract_max_timestamptz('{"a":{"b": {"c": "1980-02-05T08:30"}}}', '[["a","b","c"]]');

SELECT knife_extract_timestamptz('{"a":{"b": {"c": ["1980", "1980-02", "1980-02-05", "1980-02-05T08"]}}}', '[["a", "b", "c"]]');


SELECT knife_date_bound('2005-08-09T13:30:42Z',  'min');
SELECT knife_date_bound('2005-08-09T13:30:42Z',  'max');
SELECT knife_date_bound('2005-08-09T13:30:42+03', 'min');
SELECT knife_date_bound('2005-08-09T13:30:42+03', 'max');
