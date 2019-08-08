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
SELECT knife_date_bound('2019-07-03T02:28:57.3042803+00:00', 'min');
SELECT knife_date_bound('2019-07-03T02:28:57.3042803+00:00', 'max');

SELECT knife_date_bound('2019-07-03T02:28:57.3042803+00:00', 'min') < '2019-07-03T02:28:57.3042803+00:00'::timestamptz;
SELECT knife_date_bound('2019-07-03T02:28:57.3042803+00:00', 'max') > '2019-07-03T02:28:57.3042803+00:00'::timestamptz;


SET TIME ZONE 'UTC';
SELECT knife_date_bound('2001-01-01', 'max');
SELECT knife_date_bound('2001-01-01', 'min');
SELECT knife_extract_min_timestamptz('{"receivedTime": "2001-01-01"}', '[["receivedTime"]]');
SELECT knife_extract_max_timestamptz('{"receivedTime": "2001-01-01"}', '[["receivedTime"]]');

SELECT knife_extract_text('{"resourceType": "Some", "name": [{"use": "official", "given": ["nicola"], "family": ["Ryzhikov"]}, {"use": "common", "given": ["c", "d"]}]}', '[["name","given"],["name","family"]]');

drop table  if exists patient;
create table patient (id serial, resource jsonb);

insert into  patient (resource)
values
('{"resourceType": "Some", "name": [{"use": "official", "given": ["nicola"], "family": ["Ryzhikov"]}, {"use": "common", "given": ["c", "d"]}]}'),
('{"name": [{"given": ["Nikolai"]}], "resourceType": "Patient"}');

SELECT knife_extract_text(resource, '[["name","given"],["name","family"]]') from patient;


SELECT knife_extract(
  $$
  {"participant":
   [
    {
     "type": [{"coding": [{"code": "A1", "d": "d"},{"code": "B1"}]}],
     "individual": {"id": "i1", "rt": "pract"}
     },
    {
      "type": [{"coding": [{"code": "B1", "d": "d"},{"code": "A1", "d": "d"}]}],
      "individual": {"id": "i2", "rt": "pract"}
    },
    {
      "type": [{"coding": [{"code": "B2", "d": "d"},{"code": "A2", "d": "d"}]}],
      "individual": {"id": "i3", "rt": "pract"}
    }
   ]
  }$$,
  $$[["participant", {"type": [{"coding": [{"code": "A1"}]}]}, "individual", "id"]]$$
  );

-- empty
SELECT knife_extract(
$$
{"participant":
[
  {
  "type": [{"coding": [{"code": "A1", "d": "d"},{"code": "B1"}]}],
  "individual": {"id": "i1", "rt": "pract"}
  },
  {
  "type": [{"coding": [{"code": "B1", "d": "d"},{"code": "A1", "d": "d"}]}],
  "individual": {"id": "i2", "rt": "pract"}
  },
  {
  "type": [{"coding": [{"code": "B2", "d": "d"},{"code": "A2", "d": "d"}]}],
  "individual": {"id": "i3", "rt": "pract"}
  }
]
}$$,
$$[["participant", {"type": [{"coding": [{"code": "UPS"}]}]}, "individual", "id"]]$$
);

-- double filter
SELECT knife_extract(
$$
{"participant":
[
{
"type": [{"coding": [{"code": "A1", "d": "d"},{"code": "B"}]}],
"individual": {"id": "i1", "rt": "pract"}
},
{
"type": [{"coding": [{"code": "B1", "d": "d"},{"code": "A1", "d": "d"}]}],
"individual": {"id": "i2", "rt": "pract"}
},
{
"type": [{"coding": [{"code": "B2", "d": "d"},{"code": "A2", "d": "d"}]}],
"individual": {"id": "i3", "rt": "pract"}
}
]
}$$,
$$[["participant", {"type": [{"coding": [{"code": "A1"}, {"code": "B"}]}]}, "individual", "id"]]$$
);

-- change order
SELECT knife_extract(
$$
{"participant":
[
{
"type": [{"coding": [{"code": "B"},{"code": "A1", "d": "d"}]}],
"individual": {"id": "i1", "rt": "pract"}
},
{
"type": [{"coding": [{"code": "B1", "d": "d"},{"code": "A1", "d": "d"}]}],
"individual": {"id": "i2", "rt": "pract"}
},
{
"type": [{"coding": [{"code": "B2", "d": "d"},{"code": "A2", "d": "d"}]}],
"individual": {"id": "i3", "rt": "pract"}
}
]
}$$,
$$[["participant", {"type": [{"coding": [{"code": "A1"}, {"code": "B"}]}]}, "individual", "id"]]$$
);

SELECT knife_extract('{"a":{"b": {"c": [1,2,3,4]}}}', '[["a","b","c",0]]');

SELECT knife_extract('{"a":{"b": {"c": [1,2,3,4]}}}', '[["a","b","c",1]]');

SELECT knife_extract('{"a":{"b": {"c": [1,2,3,4]}}}', '[["a","b","c",2]]');

SELECT knife_extract('{"a":{"b": {"c": [1,2,3,4]}}}', '[["a","b","c",3]]');

SELECT knife_extract('{"a":{"b": {"c": [1,2,3,4]}}}', '[["a","b","c",4]]');

SELECT knife_extract('{"a":{"b": {"c": [1,2,3,4]}}}', '[["a","b","c",44]]');

SELECT knife_extract('{"a":{"b": [{"c": [1,2,3,4]},{"c": [11,22,33,43]}]}}',
  '[["a","b",0, "c",0]]');

SELECT knife_extract('{"a":{"b": [{"c": [1,2,3,4]},{"c": [11,22,33,43]}]}}',
'[["a","b",1, "c",0]]');

SELECT knife_extract('{"a":{"b": [{"c": [1,2,3,4]},{"c": [11,22,33,43]}]}}',
'[["a","b",2, "c",0]]');




create table test_ts (id text primary key, resource jsonb);
insert into test_ts (id,resource) values ('1', $$
{
"period": {
  "end": "2019-01-01T12:00:00",
  "start": "2018-01-01T12:00:00"
},
"status": "active",
"id": "e-1",
"meta": {
  "lastUpdated": "2019-08-08T19:07:00.242Z",
  "versionId": "147"
},
"patient": {
"display": "Pt-1"
},
"resourceType": "EpisodeOfCare"
}
$$);

SELECT knife_extract_max_timestamptz( resource , '[["period","start"], ["period","end"]]') from test_ts;
SELECT knife_extract_max_timestamptz( resource , '[["period","start"], ["period","end"]]') from test_ts;
SELECT resource from test_ts;
