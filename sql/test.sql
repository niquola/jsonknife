create extension jsonknife;

SELECT knife_extract('{"a":{"b": {"c": 5.3}}}', '["a","b","c"]'); 
SELECT knife_extract('{"a":{"b": {"c": 5.3}}}', '["a",{"d": "ups"},"c"]'); 
