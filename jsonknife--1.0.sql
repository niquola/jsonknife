-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION jsonknife" to load this file. \quit


CREATE OR REPLACE FUNCTION knife_extract(jsonb, jsonb)
RETURNS jsonb
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE;
