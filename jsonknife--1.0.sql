-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION jsonknife" to load this file. \quit

CREATE OR REPLACE FUNCTION knife_extract(jsonb, jsonb)
RETURNS jsonb[]
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

-- CREATE OR REPLACE FUNCTION knife_extract_text_array(jsonb, jsonb)
-- RETURNS text[]
-- AS 'MODULE_PATHNAME'
-- LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_extract_text(jsonb, jsonb)
RETURNS text[]
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_extract_numeric(jsonb, jsonb)
RETURNS numeric[]
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_extract_max_numeric(jsonb, jsonb)
RETURNS numeric
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_extract_min_numeric(jsonb, jsonb)
RETURNS numeric
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_extract_timestamptz(jsonb, jsonb)
RETURNS timestamptz[]
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_extract_max_timestamptz(jsonb, jsonb)
RETURNS timestamptz
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_extract_min_timestamptz(jsonb, jsonb)
RETURNS timestamptz
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;

CREATE OR REPLACE FUNCTION knife_date_bound(text, text)
RETURNS timestamptz
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;
