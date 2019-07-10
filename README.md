# jsonknife

[![Build Status](https://travis-ci.org/niquola/jsonknife.svg?branch=master)](https://travis-ci.org/niquola/jsonknife)


Useful functions for working with jsonb in PostgreSQL like data extraction, validation & transformation

## Development


```sh
git clone https://github.com/postgres/postgres
./configure --with-bonjour --prefix=/opt/local/pg
make
sudo make install

export PGDATA=/opt/pg
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

Path for data extraction from jsonb document is encoded as json vector, where

* string means object key
* number - array index
* object - is pattern to match


```json
//extracting by path:

["frends", {"best": true}, "name" ]

//from:

{"frends": [
  {"best": true, "name": "niquola"},
  {"best": true, "name": "ivan"},
  {"best": false, "name": "avraam"}
]}

// produces:

["niquola", "ivan"]
```

For more examples take a look at [tests](https://github.com/niquola/jsonknife/blob/master/expected/test.out).
