SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980"}}}', '.a.b.c', 'date', 'min');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02"}}}', '.a.b.c', 'date', 'min');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02-05"}}}', '.a.b.c', 'date', 'min');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02-05T08"}}}', '.a.b.c', 'date', 'min');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02-05T08:30"}}}', '.a.b.c', 'date', 'min');

SELECT jsonknife_extract_date('{"a":["1980-02-05T08:30", "1976-01", "1952-02-03"]}', '.a', 'date', 'min');

SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980"}}}', '.a.b.c', 'date', 'max');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02"}}}', '.a.b.c', 'date', 'max');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02-05"}}}', '.a.b.c', 'date', 'max');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02-05T08"}}}', '.a.b.c', 'date', 'max');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02-05T08:30"}}}', '.a.b.c', 'date', 'max');
SELECT jsonknife_extract_date('{"a":{"b": {"c": "1980-02-05T08:30+05"}}}', '.a.b.c', 'date', 'max');

SELECT jsonknife_extract_date('{"a":["1980-02-05T08:30", "1976-01", "1952-02-03"]}', '.a', 'date', 'max');

--- time
SELECT jsonknife_extract_date('{"time": "11:00:00" }', '.time', 'time' , 'max');
SELECT jsonknife_extract_date('{"time": "11:00:00" }', '.time', 'time' , 'min');
SELECT jsonknife_extract_date('{"time": "23:59:59" }', '.time', 'time' , 'max');
SELECT jsonknife_extract_date('{"time": "00:00:01" }', '.time', 'time' , 'min');

SELECT jsonknife_extract_date('{"time": ["11:00:00", "12:00:00", "13:00:00"] }', '.time', 'time' , 'max');
SELECT jsonknife_extract_date('{"time": ["11:00:00", "12:00:00", "13:00:00"] }', '.time', 'time' , 'min');
SELECT jsonknife_extract_date('{"time": ["23:59:59", "11:00:00", "12:00:00"] }', '.time', 'time' , 'max');
SELECT jsonknife_extract_date('{"time": ["00:00:01", "11:00:00", "12:00:00"] }', '.time', 'time' , 'min');
SELECT jsonknife_extract_date('{"time": ["00:00:01", "11:00:00", "23:59:59"] }', '.time', 'time' , 'min');
SELECT jsonknife_extract_date('{"time": ["00:00:01", "11:00:00", "23:59:59"] }', '.time', 'time' , 'max');


--- Period
SELECT jsonknife_extract_date('{"period": {"start": "1991-01-01", "end": "1991-12-31"}}', '.period', 'Period' , 'max');
SELECT jsonknife_extract_date('{"period": {"start": "1991-01-01", "end": "1991-12-31"}}', '.period', 'Period' , 'min');

SELECT jsonknife_extract_date('{"period": {"start": "1991-01-01"}}', '.period', 'Period' , 'max');
SELECT jsonknife_extract_date('{"period": {"start": "1991-01-01"}}', '.period', 'Period' , 'min');

SELECT jsonknife_extract_date('{"period": {"end": "1991-01-01"}}', '.period', 'Period' , 'max');
SELECT jsonknife_extract_date('{"period": {"end": "1991-01-01"}}', '.period', 'Period' , 'min');

SELECT jsonknife_extract_date('{"period": [{"start": "1991-01-01"},
                                     {"start": "1991-01-01", "end": "1991-12-31"},
                                     {"end":   "1991-12-31"}]}', '.period', 'Period' , 'max');
SELECT jsonknife_extract_date('{"period": [{"start": "1991-01-01"},
                                     {"start": "1991-01-01", "end": "1991-12-31"},
                                     {"end":   "1991-12-31"}]}', '.period', 'Period' , 'min');

select '''{
  "Period":{
    "value": {"start": "1991-01-01", "end": "1991-12-31"},
    "array": [{"start": "1991-01-01", "end": "1991-12-31"},
 						  {"start": "1990-01-01", "end": "1991-12-31"},
							{"start": "1981-01-01"}],
    "where": [{ "code": "value", "value": {"start": "1991-01-01", "end": "1991-12-31"} },
              { "code": "array", "array": [{"start": "1991-01-01", "end": "1991-12-31"},
																					 {"start": "1990-01-01", "end": "1991-12-31"},
																					 {"start": "1981-01-01"}] },
              { "code": "where",
                "where": [ {"code": "value", "value": {"start": "1991-01-01", "end": "1991-12-31"}},
                           {"code": "array", "array": [{"start": "1991-01-01", "end": "1991-12-31"},
																											 {"start": "1990-01-01", "end": "1991-12-31"},
																											 {"start": "1981-01-01"}]} ] } ] }
}''' resource \gset

SELECT jsonknife_extract_date(:resource, '.Period.value', 'Period' , 'max');
SELECT jsonknife_extract_date(:resource, '.Period.array', 'Period' , 'max');
SELECT jsonknife_extract_date(:resource, '.Period.where.where(code=value).value', 'Period' , 'max');
SELECT jsonknife_extract_date(:resource, '.Period.where.where(code=array).array', 'Period' , 'max');
SELECT jsonknife_extract_date(:resource, '.Period.where.where(code=where).where.where(code=value).value', 'Period' , 'max');
SELECT jsonknife_extract_date(:resource, '.Period.where.where(code=where).where.where(code=array).array', 'Period' , 'max');

--- Timing
SELECT jsonknife_extract_date('{"timing": {"event": ["1992-12-31"]}}', '.timing', 'Timing' , 'max');
SELECT jsonknife_extract_date('{"timing": {"event": ["1992-10-10"]}}', '.timing', 'Timing' , 'max');
SELECT jsonknife_extract_date('{"timing": {"event": ["1991-01-01"]}}', '.timing', 'Timing' , 'min');
SELECT jsonknife_extract_date('{"timing": {"event": ["1991-01-04"]}}', '.timing', 'Timing' , 'min');

SELECT jsonknife_extract_date('{"timing": {"event": ["1993-01-01", "1992-12-31"]}}', '.timing', 'Timing' , 'max');
SELECT jsonknife_extract_date('{"timing": {"event": ["1993-01-01", "1992-12-31"]}}', '.timing', 'Timing' , 'min');

SELECT jsonknife_extract_date('{"timing": [{"event": ["1992-12-31"]}, {"event": ["1993-12-31"]}, {"event": ["1994-12-31"]}]}', '.timing', 'Timing' , 'max');
SELECT jsonknife_extract_date('{"timing": [{"event": ["1992-01-01"]}, {"event": ["1993-12-31"]}, {"event": ["1994-12-31"]}]}', '.timing', 'Timing' , 'min');

SELECT jsonknife_extract_date('{"timing": [{"event": ["1991-01-01", "1992-12-31"]}, {"event": ["1990-01-01", "1995-12-31"]}, {"event": ["1994-12-31"]}]}', '.timing', 'Timing' , 'max');
SELECT jsonknife_extract_date('{"timing": [{"event": ["1987-01-01", "1992-12-31"]}, {"event": ["1990-01-01", "1995-12-31"]}, {"event": ["1994-12-31"]}]}', '.timing', 'Timing' , 'min');

select '''{
  "Timing":{
    "value": {"event": ["1991-01-01", "1992-01-01"]},
    "array": [{"event": ["1991-01-01"]},
						  {"event": ["2000-01-01", "1990-01-01"]},
						  {"event": ["1992-01-01"]}],
    "where": [{ "code": "value", "value": {"event": ["1991-01-01", "1992-01-01"]} },
              { "code": "array", "array": [{"event": ["1991-01-01"]},
																					 {"event": ["2000-01-01", "1990-01-01"]},
																					 {"event": ["1992-01-01"]}] },
              { "code": "where",
                "where": [ {"code": "value", "value": {"event": ["1991-01-01", "1992-01-01"]}},
                           {"code": "array", "array": [{"event": ["1991-01-01"]},
																											 {"event": ["2000-01-01", "1990-01-01"]},
																											 {"event": ["1992-01-01"]}]} ] } ] }
}''' resource \gset
SELECT jsonknife_extract_date(:resource, '.Timing.value', 'Timing' , 'max');
SELECT jsonknife_extract_date(:resource, '.Timing.array', 'Timing' , 'max');
SELECT jsonknife_extract_date(:resource, '.Timing.where.where(code=value).value', 'Timing' , 'max');
SELECT jsonknife_extract_date(:resource, '.Timing.where.where(code=array).array', 'Timing' , 'max');
SELECT jsonknife_extract_date(:resource, '.Timing.where.where(code=where).where.where(code=value).value', 'Timing' , 'max');
SELECT jsonknife_extract_date(:resource, '.Timing.where.where(code=where).where.where(code=array).array', 'Timing' , 'max');

SET TIME ZONE '+03';


SELECT fhirpath_date_bound('2005-08-09T13:30:42Z',  'min');
SELECT fhirpath_date_bound('2005-08-09T13:30:42Z',  'max');
SELECT fhirpath_date_bound('2005-08-09T13:30:42+03', 'min');
SELECT fhirpath_date_bound('2005-08-09T13:30:42+03', 'max');

SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00+03"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00Z"}', '.a', 'date', 'min');

SET TIME ZONE 'utc';


SELECT fhirpath_date_bound('2005-08-09T13:30:42Z',  'min');
SELECT fhirpath_date_bound('2005-08-09T13:30:42Z',  'max');
SELECT fhirpath_date_bound('2005-08-09T13:30:42+03', 'min');
SELECT fhirpath_date_bound('2005-08-09T13:30:42+03', 'max');

SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00+03"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00Z"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00Z"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00Z"}', '.a', 'date', 'min');

SELECT jsonknife_extract_date('{"a": "2005-08-09T13"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:0"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:0"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T13:00:00Z"}', '.a', 'date', 'min');

SELECT jsonknife_extract_date('{"a": "2005-08-09T11:00:00+11"}', '.a', 'date', 'min');
SELECT jsonknife_extract_date('{"a": "2005-08-09T11:00:00+11"}', '.a', 'date', 'max');

SELECT fhirpath_date_bound('2005-08-09T11:00:00+11', 'min');
SELECT fhirpath_date_bound('2005-08-09T11:00:00+11', 'max');

SELECT jsonknife_extract_date('{"a": "2005-08-09T11:00:00+11"}', '.a', 'date', 'max') <= fhirpath_date_bound('2005-08-09T11:00:00+11', 'max');
SELECT jsonknife_extract_date('{"a": "2005-08-09T11:00:00+11"}', '.a', 'date', 'min') >= fhirpath_date_bound('2005-08-09T11:00:00+11', 'min');

SELECT jsonknife_extract_date('{"a": {"start": "2005-08-09T11:00:00+11"}}', '.a', 'Period', 'min');
SELECT jsonknife_extract_date('{"a": {"start": "2005-08-09T11:00:00+11"}}', '.a', 'Period', 'min') >= fhirpath_date_bound('2005-08-09T11:00:00+11', 'min');

SELECT jsonknife_extract_date('{"a": {"end": "2005-08-09T12:00:00+11"}}', '.a', 'Period', 'max');
SELECT jsonknife_extract_date('{"a": {"end": "2005-08-09T12:00:00+11"}}', '.a', 'Period', 'max') <= fhirpath_date_bound('2005-08-09T12:00:00+11', 'min');

SELECT fhirpath_date_bound('2005-08-09T09:00:00+11', 'min');

-- should not be 2006
SELECT fhirpath_date_bound('2005', 'max');
SELECT fhirpath_date_bound('2005-03', 'max');
SELECT fhirpath_date_bound('2005-02', 'max');
SELECT fhirpath_date_bound('2005-03-03', 'max');
SELECT fhirpath_date_bound('2005-03-03T10:00', 'max');
SELECT fhirpath_date_bound('2005-03-03T10', 'max');
SELECT fhirpath_date_bound('2005-03-03T10:00:00', 'max');
SELECT fhirpath_date_bound('2005-03-03T10:00:00.5', 'max');
SELECT fhirpath_date_bound('2005-03-03T10:00:00.55', 'max');
SELECT fhirpath_date_bound('2005-03-03T10:00:00.555', 'max');
SELECT fhirpath_date_bound('2005-03-03T10:00:00.5555', 'max');
SELECT fhirpath_date_bound('2005-03-03T10:00:00.55555', 'max');


-- should not interesect
SELECT fhirpath_date_bound('2005', 'max') < fhirpath_date_bound('2006', 'min');

SELECT fhirpath_date_bound('1979', 'max');
SELECT fhirpath_date_bound('1980', 'min');
SELECT fhirpath_date_bound('1979', 'max') < fhirpath_date_bound('1980', 'min');


SELECT jsonknife_extract_date('{"propPeriod": {"end": "2005-08-09T12:00:00+11"}}', '.prop', 'Polymorphic', 'max');
-- poly
select jsonknife_extract_date('{"performedPeriod": {"start": "1991-01-01", "end": "1991-12-31"}}', '.performed', 'Polymorphic', 'max');
select jsonknife_extract_date('{"performedDateTime": "1980-02-05T08:30"}', '.performed', 'Polymorphic', 'min');

select jsonknife_extract_date('{"arr": [
{"code":"code_1", "performedPeriod": {"start": "1993-01-01", "end": "1993-12-31"}},
{"code":"code_2", "performedPeriod": {"start": "1991-01-01", "end": "1991-12-31"}}
]}', '.arr.where(code=code_1).performed', 'Polymorphic', 'max');

SELECT jsonknife_extract_date('{"activity": {"detail": {"scheduledTiming": {"event": ["2017-02-12T00:00:00.000Z"]}}}}', '.activity.detail.scheduled', 'Polymorphic', 'max');
SELECT jsonknife_extract_date('{"activity": {"detail": {"scheduledString": "some human date as string"}}}', '.activity.detail.scheduled', 'Polymorphic', 'max');
SELECT jsonknife_extract_date('{"activity": {"detail": {"scheduled": {"event": ["2017-02-12T00:00:00.000Z"]}}}}', '.activity.detail.scheduled', 'Polymorphic', 'max') is null;
