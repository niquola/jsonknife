#!/bin/bash
set -e

# usage: file_env VAR [DEFAULT]
#    ie: file_env 'XYZ_DB_PASSWORD' 'example'
# (will allow for "$XYZ_DB_PASSWORD_FILE" to fill in the value of
#  "$XYZ_DB_PASSWORD" from a file, especially for Docker's secrets feature)
file_env() {
	local var="$1"
	local fileVar="${var}_FILE"
	local def="${2:-}"
	if [ "${!var:-}" ] && [ "${!fileVar:-}" ]; then
		echo >&2 "error: both $var and $fileVar are set (but are exclusive)"
		exit 1
	fi
	local val="$def"
	if [ "${!var:-}" ]; then
		val="${!var}"
	elif [ "${!fileVar:-}" ]; then
		val="$(< "${!fileVar}")"
	fi
	export "$var"="$val"
	unset "$fileVar"
}

PATH=/pg/bin/:$PATH

if [ "${1:0:1}" = '-' ]; then
	  set -- postgres "$@"
fi


PGDATA=/data

if [ ! -s "/data/PG_VERSION" ]; then

  mkdir -p /data
  chmod 700 /data
  chown -R postgres /data

	file_env 'POSTGRES_INITDB_ARGS'
	su - postgres -c "/pg/bin/pg_ctl initdb -D /data"

	# check password first so we can output the warning before postgres
	# messes it up
	file_env 'POSTGRES_PASSWORD'
  pass="PASSWORD '$POSTGRES_PASSWORD'"
  authMethod=md5

  { echo; echo "host all all all $authMethod"; } | tee -a "$PGDATA/pg_hba.conf" > /dev/null

	su - postgres -c '/pg/bin/pg_ctl -D /data  -w start'
	su - postgres -c '/pg/bin/createuser -s root'

  echo "ALTER USER postgres WITH SUPERUSER $pass" | /pg/bin/psql postgres

	# Some tweaks to default configuration
	cat <<-CONF >> /data/postgresql.conf
		listen_addresses = '*'
		shared_preload_libraries='pg_pathman'
		synchronous_commit = off
		shared_buffers = '2GB'
		max_wal_size = '4GB'
	CONF

  chown postgres:postgres /data/postgresql.conf

	su - postgres -c '/pg/bin/pg_ctl -D /data -m fast -w stop'

	echo
	echo 'PostgreSQL init process complete; ready for start up.'
	echo
fi

# allow the container to be started with `--user`
if [ "$1" = 'postgres' ] && [ "$(id -u)" = '0' ]; then
	  mkdir -p /data
	  chown -R postgres /data
	  chmod 700 /data

    echo "postgres & 0"
	  exec gosu postgres "$@"

else
    exec "$@"
fi


