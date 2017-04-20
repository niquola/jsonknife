# jsonknife

[![Build Status](https://travis-ci.org/niquola/jsonknife.svg?branch=master)](https://travis-ci.org/niquola/jsonknife)


Useful functions for working with jsonb in PostgreSQL like data extraction, validation & transformation

## Development


```sh
git clone https://github.com/postgres/postgres
./configure --with-bonjour --prefix=/opt/local/pg
make
sudo make install

export PGDATA=/tmp/pg
export PGPORT=5777\n
export PG_BIN=/opt/local/pg/bin\n
bin/initdb -D $PGDATA -E utf8
vim /tmp/pg/postgresql.conf # check port configure log /tmp/pg.log
/opt/local/pg/bin/pg_ctl start -D $PGDATA

cd postgres && src/tools/make_etags
cd postgres/src/contrib
git clone https://github.com/niquola/jsonknife
cd jsonknife
..postgres/src/tools/make_etags .

make && sudo make install && make installcheck

````

## Examples


```sql

SELECT knife_extract_jsonb('{"a": {"b": [{"c": 1}, {"c": 2}]}}', '["a", "b", "c"]')

=> '[1,2]'::jsonb

SELECT knife_extract_string(
  '{"name": [{"use": "official", "given": ["Abram"]}]',
  '["name", {"use": "official"}, "given"]'
);


```


