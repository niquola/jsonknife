create extension jsonknife;

SELECT knife_extract('{"a":{"b": {"c": 5.3}}}', '[["a","b","c"]]'); 

SELECT knife_extract('{"a":[{"b": [{"c": 5.3}, {"c": 6.3}]}, {"b": [{"c": 5.3}, {"c": 6.3}]}]}', '[["a","b","c"]]'); 

SELECT knife_extract('{"a": [1, 2, {"b": {"c": 5.3}}, {"b": {"c": 100}}]}', '[["a", 2 ,"b", "c"]]'); 

SELECT knife_extract('{"name": [{"use": "official", "given": ["a", "b"]}, {"use": "common", "given": ["c", "d"]}]}', '[["name",{"use": "official"},"given"]]');
SELECT knife_extract('{"name": [{"use": "official", "given": ["a", "b"]}, {"use": "official", "given": ["c", "d"]}]}', '[["name",{"use": "official"},"given"]]'); 

SELECT knife_extract('{"name": [{"a": {"b": 1}, "c": "ok"}, {"a": {"b": 2}, "c": "fail"}]}', '[["name", {"a":{"b":1}}, "c"]]');



SELECT knife_extract('{"a":{"b": {"c": 5.3, "d": 6.7}}}', '[["a","b","c"],["a","b","d"]]'); 
