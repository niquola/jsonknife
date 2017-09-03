/*-------------------------------------------------------------------------
 *
 * jsonknife.c
 *	  useful functions for working with jsonb in PostgreSQL
 *
 * Any interface function calls the reduce_paths() function,
 * which extracts queries from the second argument(paths),
 * and passes them all one by one to the main execution
 * function reduce_path().
 *
 * Default behavior on wrong query input: search by tag PASSING BY
 *
 * TODO:
 * - TimeStamp functions
 * - jsonbv_type() refactoring 
 * - support of nested arrays in path query
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/datetime.h"
#include "utils/formatting.h"
#include "utils/json.h"
#include "utils/jsonb.h"
#include "utils/timestamp.h"
/*#include "utils/varlena.h"*/

typedef enum MinMax {min, max} MinMax;  

typedef enum JValueType
{
	JVObject,
	JVArray,
	JVString,
	JVNumeric,
	JVBoolean,
	JVNull
} JValueType; 

/*
 * Accumulators structures below.
 */
typedef struct BasicAccumulator
{
} BasicAccumulator;

typedef struct NumericAccumulator
{
	MinMax minmax; /* condition */
	Numeric acc; /* singleton */
} NumericAccumulator;

typedef struct ArrayAccumulator
{
	bool case_insensitive; /* for future use */
	ArrayBuildState *acc; /* holding elements of an array*/
} ArrayAccumulator;

/* for future use */
typedef struct DateAccumulator
{
	MinMax minmax;
	Datum  acc;
} DateAccumulator;

/* for future use */
typedef struct TextAccumulator
{
	text *acc;
} TextAccumulator;

/* for future use */
typedef struct BoolAccumulator
{
	bool acc;
} BoolAccumulator;

typedef void (*reduce_fn)(void *acc, JsonbValue *val);

static text *jsonbv_to_text(JsonbValue *v);
static void initJsonbValue(JsonbValue *jbv, Jsonb *jb);
static bool knife_match(JsonbValue *value, JsonbValue *pattern);
static void update_numeric(NumericAccumulator *nacc, Numeric num);
static Datum date_bound(char *date_str, long str_len,  MinMax minmax);

static MinMax minmax_from_string(char *s);

/*
 * Cast JsonbValue type to JValueType
 *
 * NOTE: actually, usefull only of jbvBinary
 */
static JValueType
jsonbv_type(JsonbValue *v)
{
	JsonbIterator *array_it;
	JsonbValue	array_value;
	int next_it;

	if (v == NULL) return JVNull;

	switch (v->type)
	{
		case jbvNull:
			return JVNull;
		case jbvBool:
			return JVBoolean;
		case jbvString:
			return JVString;
		case jbvNumeric:
			return JVNumeric;
		case jbvBinary:
		case jbvArray:
		case jbvObject:
		{
			array_it = JsonbIteratorInit((JsonbContainer *) v->val.binary.data);
			next_it = JsonbIteratorNext(&array_it, &array_value, true);
			
			if (next_it == WJB_BEGIN_ARRAY)
				return JVArray;
			else if (next_it == WJB_BEGIN_OBJECT)
				return JVObject;
		}
		default:
			return JVNull;
	}
}

/*
 * used in the next two functions
 */
static inline StringInfoData *
append_jsonbv_to_buffer(StringInfoData *out, JsonbValue *v)
{
	if (out == NULL)
		out = makeStringInfo();

	switch (v->type)
	{
		case jbvNull:
			return NULL;
		case jbvBool:
			appendStringInfoString(out, (v->val.boolean ? "true" : "false"));
			break;
		case jbvString:
			appendBinaryStringInfo(out, v->val.string.val, v->val.string.len);
			break;
		case jbvNumeric:
			appendStringInfoString(out,
								   DatumGetCString(
									   DirectFunctionCall1(numeric_out,
														   PointerGetDatum(v->val.numeric))));
			break;
		case jbvBinary:
		case jbvArray:
		case jbvObject:
		{
			Jsonb *binary = JsonbValueToJsonb(v);
			(void) JsonbToCString(out, &binary->root, -1);
			break;
		}
		default:
			elog(ERROR, "Wrong jsonb type: %d", v->type);
	}
	
	return out;
}

/*
 * convert JsonbValue to cstring
 */
static inline char *
jsonbv_to_string(JsonbValue *v)
{
	StringInfoData *out;
	
	out = makeStringInfo();
	append_jsonbv_to_buffer(out, v);
	return out->data;
}

/*
 * convert JsonbValue to text (varlena type)
 */
text *
jsonbv_to_text(JsonbValue *v)
{
	StringInfoData *out;
	
	if (v == NULL) 
		return NULL;

	out = makeStringInfo();
	/*
	 * construct varlena type (text) in out->data
	 * - reserve space for a varlena header 
	 * - write data
	 * - set length on reserved space
	 */
	appendStringInfoSpaces(out, VARHDRSZ);
	append_jsonbv_to_buffer(out, v);
	SET_VARSIZE(out->data, out->len);
	return (text *)out->data;
}

void
initJsonbValue(JsonbValue *jbv, Jsonb *jb)
{
	jbv->type = jbvBinary;
	jbv->val.binary.data = &jb->root;
	jbv->val.binary.len = VARSIZE_ANY_EXHDR(jb);
}

/* return value of obj key */
static inline JsonbValue *
jsonb_get_key(JsonbValue *obj, JsonbValue *key)
{
	if (obj->type == jbvBinary)
		return findJsonbValueFromContainer((JsonbContainer *) obj->val.binary.data,
										   JB_FOBJECT,
										   key);
	else
		return NULL;
}

/*
 * Compare two scalar JsonbValues, returning -1, 0, or 1.
 *
 * Strings are compared using the default collation.  Used by B-tree
 * operators, where a lexical sort order is generally expected.
 */
static int
compareJsonbScalarValue(JsonbValue *aScalar, JsonbValue *bScalar)
{
	if (aScalar->type == bScalar->type)
	{
		switch (aScalar->type)
		{
			case jbvNull:
				return 0;
			case jbvString:
				return varstr_cmp(aScalar->val.string.val,
								  aScalar->val.string.len,
								  bScalar->val.string.val,
								  bScalar->val.string.len,
								  DEFAULT_COLLATION_OID);
			case jbvNumeric:
				return DatumGetInt32(DirectFunctionCall2(
										 numeric_cmp,
										 PointerGetDatum(aScalar->val.numeric),
										 PointerGetDatum(bScalar->val.numeric)));
			case jbvBool:
				if (aScalar->val.boolean == bScalar->val.boolean)
					return 0;
				else if (aScalar->val.boolean > bScalar->val.boolean)
					return 1;
				else
					return -1;
			default:
				elog(ERROR, "invalid jsonb scalar type");
		}
	}
	elog(ERROR, "jsonb scalar type mismatch");
	return -1;
}

bool
knife_match(JsonbValue *value, JsonbValue *pattern)
{
	JValueType vtype = jsonbv_type(value);
	JValueType ptype = jsonbv_type(pattern);

	if (value == NULL || pattern == NULL)
		return false;

	if ((vtype == JVString || vtype == JVNumeric || vtype == JVBoolean)
		&& (ptype == JVString || ptype == JVNumeric || ptype == JVBoolean))
	{
    /* elog(INFO, "compare prim %d, %s with %s", */
    /*      compareJsonbScalarValue(value, pattern), */
    /*      jsonbv_to_string(NULL, value), */
    /*      jsonbv_to_string(NULL, pattern)); */

		if(compareJsonbScalarValue(value, pattern) == 0)
			return true;
		else
			return false;
	}

	if( vtype == JVObject && ptype == JVObject)
	{
		/* elog(INFO, "compare objects"); */
		/* JsonbValue *path_item; */
		JsonbIterator *array_it;
		JsonbValue	array_value;
		JsonbValue *sample_value = NULL;
		int next_it;
		bool matched = true;

		array_it = JsonbIteratorInit((JsonbContainer *) pattern->val.binary.data);
		next_it = JsonbIteratorNext(&array_it, &array_value, true);

		/* PASSING BY */
		if (!array_it->nElems)
			elog(INFO, "empty object found, matched = true");

		while (matched == true
			   && (next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE)
		{
			if (next_it == WJB_KEY)
			{
				/* elog(INFO, "compare keys"); */
				sample_value = jsonb_get_key(value, &array_value);
				if (sample_value == NULL)
					matched = false;
			}
			else if ( next_it == WJB_VALUE )
			{
				/* elog(INFO, "compare vals %s: %s", sample_value, &array_value); */
				if (!knife_match(sample_value, &array_value))
					matched = false;
			}
		}
		/* elog(INFO, "matched %d", matched); */
		return matched;
	}
	return false;
}

/*
 * Main executor function: it steps through the elements of the path
 * and cut corresponding part of JsonbValue (jbv)
 *
 * jbv - jsonb object to query
 * path - query, array of JsonbValue
 * current_idx - current element of query 
 * path_len - total number of elements in query (array length)
 * acc - accumulator; to hold intermediate results
 * reduce_fn - function; to work with accumulator
 */
static long
reduce_path(JsonbValue *jbv, JsonbValue **path, int current_idx, int path_len, void *acc, reduce_fn fn)
{
	JsonbValue *next_v = NULL;
	JsonbValue *path_item = NULL;
	JsonbIterator *array_it;
	JsonbValue	array_value;
	int next_it;

	long num_results = 0;

	check_stack_depth();
	/*elog(INFO, "enter with: %s", jsonbv_to_string(jbv));*/

	/*
	 * if JsonbValue is empty then nothing to do
	 */
	if (jbv == NULL) return 0;

	Assert(jbv != NULL);
	path_item = path[current_idx];

	/*
	 * Unwrap an array if path_item is not a number
	 * NOTE: even at the peak of the knife
	 */
	if (jsonbv_type(jbv) == JVArray
	   && (path_item == NULL || jsonbv_type(path_item) != JVNumeric))
	{
		array_it = JsonbIteratorInit((JsonbContainer *) jbv->val.binary.data);
		next_it = JsonbIteratorNext(&array_it, &array_value, true);

		while ((next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE)
		{
			if (next_it == WJB_ELEM)
				num_results += reduce_path(&array_value, path, current_idx, path_len, acc, fn);
		}
		return num_results;
	}

	/*
	 *  at the peak of the knife query reduce_fn is called
	 */
	Assert(jbv != NULL);
	if (current_idx == path_len)
	{
		fn(acc, jbv);
		return 1;
	}

	Assert(current_idx < path_len);
	switch (jsonbv_type(path_item))
	{
		case JVString:
			/* elog(INFO, " in key: %s", jsonbv_to_string(path_item)); */
			if (jsonbv_type(jbv) == JVObject)
			{
				next_v = jsonb_get_key(jbv, path_item);
				if (next_v != NULL)
					num_results += reduce_path(next_v, path, (current_idx + 1), path_len, acc, fn);
			}
			break;
		case JVNumeric:
			/* elog(INFO, " in index: %s", jsonbv_to_string(path_item)); */
			if (jsonbv_type(jbv) == JVArray)
			{
				int array_index = 0;
				int required_index = DatumGetInt32(
					DirectFunctionCall1(
						numeric_int4,
						NumericGetDatum(path_item->val.numeric)));

				/* PASSING BY */
				if (required_index < 0)
				{
					elog(INFO, "out of range: %d, ignored", required_index);
					num_results += reduce_path(jbv, path, (current_idx + 1), path_len, acc, fn);
					break;
				}
				
				array_it = JsonbIteratorInit((JsonbContainer *) jbv->val.binary.data);
				next_it = JsonbIteratorNext(&array_it, &array_value, true);
				/* elog(INFO, "looking for index %d", required_index); */
				while ((next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE
					   && array_index != -1)
				{
					if(next_it == WJB_ELEM)
					{
						if(array_index == required_index)
						{
							num_results += reduce_path(&array_value, path, (current_idx +1), path_len, acc, fn);
							array_index = -1;
						}
					}
					array_index++;
				}
			}
			break;
		case JVObject:
			if (jsonbv_type(jbv) == JVObject)
			{
				if (knife_match(jbv, path_item))
				{
					/* elog(INFO, "matched"); */
					num_results += reduce_path(jbv, path, (current_idx + 1), path_len, acc, fn);
				}
			}
			break;
		case JVArray:
		case JVBoolean:
		case JVNull:
			/* PASSING BY */
			elog(INFO, "Array, boolean types are not implemented, ignored");
			num_results += reduce_path(jbv, path, (current_idx + 1), path_len, acc, fn);
			break;
	}
	return num_results;
}

static void
reduce_jsonb_array(void *acc, JsonbValue *val)
{
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;
	/* elog(INFO, "leaf: %s", jsonbv_to_string(val)); */

	if ( val != NULL )
		tacc->acc = accumArrayResult(tacc->acc,
									 (Datum) JsonbValueToJsonb(val),
									 false,
									 JSONBOID,
									 CurrentMemoryContext);
}

/*
 * Transform path_item to an array of JsonbValue values
 */
static int
to_json_path(JsonbValue *arr, JsonbValue **pathArr)
{
	int path_len = 0;
	JsonbIterator *iter;
	JsonbValue	*item;
	int next_it;

	if (arr != NULL && arr->type == jbvBinary)
	{
		item = palloc(sizeof(JsonbValue));
		iter = JsonbIteratorInit((JsonbContainer *) arr->val.binary.data);
		next_it = JsonbIteratorNext(&iter, item, true);

		if (next_it == WJB_BEGIN_ARRAY)
		{
			while ((next_it = JsonbIteratorNext(&iter, item, true)) != WJB_DONE)
			{
				/* elog(INFO, "next: %s", jsonbv_to_string(NULL, item)); */
				if (next_it == WJB_ELEM)
				{
					pathArr[path_len] = item;
					path_len++;
				}
				item = palloc(sizeof(JsonbValue));
			}
		}
		pathArr[path_len + 1] = NULL;
		return path_len;
	}
	return 0;
}

static int
reduce_paths(Jsonb *value, Jsonb *paths, void *acc, reduce_fn fn)
{
	JsonbValue jdoc;
	JsonbValue jpaths;
	
	JsonbIterator *path_iter;
	JsonbValue	path_item;
	int next_it;

	JsonbValue *pathArr[100];
	int path_len;

	long num_results = 0;

	if (JB_ROOT_IS_SCALAR(paths))
	{
		elog(INFO, "scalar input is not implemented");
		return num_results;
	}
	else if (JB_ROOT_IS_OBJECT(paths))
	{
		elog(INFO, "object input is not implemented");
		return num_results;
	}
	else
	{
		Assert(JB_ROOT_IS_ARRAY(paths));

		initJsonbValue(&jdoc, value);
		initJsonbValue(&jpaths, paths);

		path_iter = JsonbIteratorInit((JsonbContainer *) jpaths.val.binary.data);
		next_it = JsonbIteratorNext(&path_iter, &path_item, true);

		Assert(next_it == WJB_BEGIN_ARRAY);
		
		if (next_it == WJB_BEGIN_ARRAY)
		{
			while ((next_it = JsonbIteratorNext(&path_iter, &path_item, true)) != WJB_DONE)
			{
				if (next_it == WJB_ELEM)
				{
					/* PASSING BY */
					if (jsonbv_type(&path_item) != JVArray)
					{
						elog(INFO, "array queries are supported only");
						continue;
					}
					path_len = to_json_path(&path_item, pathArr);
					num_results += reduce_path(&jdoc, pathArr, 0, path_len, acc, fn);
				}
			}
		}
	}
	return num_results;
}

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(knife_extract);
Datum
knife_extract(PG_FUNCTION_ARGS)
{
	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	long num_results;

	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	num_results = reduce_paths(value, paths, &acc, reduce_jsonb_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}

/*-------------------------------------------------------------------------
 * work with text
 */

static void
reduce_text_array(void *acc, JsonbValue *val)
{
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;
	/* elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */

	if (val != NULL && jsonbv_type(val) == JVString )
	{
				tacc->acc = accumArrayResult(tacc->acc,
									 (Datum) jsonbv_to_text(val),
									 false,
									 TEXTOID,
									 CurrentMemoryContext);
	}
	/* PASSING BY otherwise*/
}


PG_FUNCTION_INFO_V1(knife_extract_text);
Datum
knife_extract_text(PG_FUNCTION_ARGS)
{

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	long num_results;
	
	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	num_results = reduce_paths(value, paths, &acc, reduce_text_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}

/*-------------------------------------------------------------------------
 * work with numbers
 */

/*
 * extract number from JsonbValue (val) and update ArrayAccumulator
 */
static void
reduce_numeric_array(void *acc, JsonbValue *val)
{
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;

	/* elog(INFO, "leaf: %s", jsonbv_to_string( val)); */

	if (val != NULL && jsonbv_type(val) == JVNumeric)
	{
		tacc->acc = accumArrayResult(tacc->acc,
									 (Datum) val->val.numeric,
									 false,
									 NUMERICOID,
									 CurrentMemoryContext);
	}
	/* PASSING BY otherwise*/
}

/*
 * extract numbers matched the queries, array of numbers returned
 */
PG_FUNCTION_INFO_V1(knife_extract_numeric);
Datum
knife_extract_numeric(PG_FUNCTION_ARGS)
{

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	long num_results; 

	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	num_results = reduce_paths(value, paths, &acc, reduce_numeric_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}

/*-------------------------------------------------------------------------
 * work with NumericAccumulator 
 */

/*
 * update NumericAccumulator
 *
 * update content of accumulator if condition holds
 */
static void
update_numeric(NumericAccumulator *nacc, Numeric num)
{
	if (nacc->acc == NULL)
		nacc->acc = num;
	else
	{
		bool gt = DirectFunctionCall2(numeric_gt, (Datum) nacc->acc, (Datum) num);
		/*elog(INFO, "%s > %s, gt %d min %d",
				numeric_to_cstring(num),
				numeric_to_cstring(nacc->acc),
				gt,
				nacc->minmax); */
		if (nacc->minmax == min && gt == 1)
			nacc->acc = num;
		else if (nacc->minmax == max && gt == 0)
			nacc->acc = num;
	}
}

/*
 * extract number from JsonbValue (val) and update NumericAccumulator
 *
 * - if val is Null then nothing to do
 * - if val is a number then return it
 * - if val is a string then cast to number and return result
 * - otherwise raise an error
 */
static void
reduce_numeric(void *acc, JsonbValue *val)
{
	NumericAccumulator *nacc = acc;
	/* elog(INFO, "extract number %s", jsonbv_to_string(val)); */
	if (val == NULL) return;

	if (val->type == jbvNumeric)
		update_numeric(nacc, val->val.numeric);
	else if (val->type == jbvString)
	{
		/*
		long len = val->val.string.len;
		char *num_str = palloc(len + 1);
		memcpy(num_str, val->val.string.val, len);
		num_str[len] = '\0';
		*/
		update_numeric(nacc, DatumGetNumeric(
						   DirectFunctionCall3(numeric_in,
											   CStringGetDatum(jsonbv_to_string(val)),
											   0,
											   -1)));
	}
	else
		elog(ERROR, "Could not extract as a number %s", jsonbv_to_string(val));
}

/*
 * Extract max number matched on of the path queries
 */
PG_FUNCTION_INFO_V1(knife_extract_max_numeric);
Datum
knife_extract_max_numeric(PG_FUNCTION_ARGS)
{

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	NumericAccumulator acc;
	acc.minmax = max;
	acc.acc = NULL;

	reduce_paths(value, paths, &acc, reduce_numeric);

	if (acc.acc != NULL)
		PG_RETURN_NUMERIC(acc.acc);
	else
		PG_RETURN_NULL();
}

/*
 * Extract min number matched on of the path queries
 */
PG_FUNCTION_INFO_V1(knife_extract_min_numeric);
Datum
knife_extract_min_numeric(PG_FUNCTION_ARGS)
{

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	NumericAccumulator acc;
	acc.minmax = min;
	acc.acc = NULL;

	reduce_paths(value, paths, &acc, reduce_numeric);

	if (acc.acc != NULL)
		PG_RETURN_NUMERIC(acc.acc);
	else
		PG_RETURN_NULL();
}

/*-------------------------------------------------------------------------
 * FIXME: work with dates
 */

Datum
date_bound(char *date_str, long str_len,  MinMax minmax)
{
	if(date_str != NULL)
	{
		char *ref_str = "0000-01-01T00:00:00";
		long ref_str_len = strlen(ref_str);

		long date_in_len = (str_len > ref_str_len) ? str_len : ref_str_len;
		char *date_in = palloc(date_in_len + 1);

		Datum min_date;
		
		memcpy(date_in, date_str, str_len);

		/* elog(INFO, "date_str: '%s', %d", date_str, str_len ); */

		if( str_len < ref_str_len)
		{
			long diff = (ref_str_len - str_len);
			memcpy(date_in + str_len, ref_str + str_len, diff);
		}

		date_in[date_in_len] = '\0';

		/* elog(INFO, "input: '%s', %d, %d", date_in, date_in_len, ref_str_len); */

		min_date = DirectFunctionCall3(timestamptz_in,
									   CStringGetDatum(date_in),
									   ObjectIdGetDatum(InvalidOid),
									   Int32GetDatum(-1));
		if(minmax == min)
			return min_date;
		else if (minmax == max )
		{
			Timestamp	max_date;
			int			tz;
			struct pg_tm tt, *tm = &tt;
			fsec_t		fsec;
			int fsec_up = 0, temp, count = 1;

			if (timestamp2tm(min_date, &tz, tm, &fsec, NULL, NULL) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
						 errmsg("timestamp out of range")));

			/* elog(INFO, "get tm %d y %d m %d d %d fsec", tm->tm_year, tm->tm_mon, tm->tm_mday, fsec); */

			if (str_len < 5){
				tm->tm_mon = 12;
			}
			if (str_len < 8){
				tm->tm_mday = day_tab[isleap(tm->tm_year)][tm->tm_mon - 1];
			}
			if (str_len < 11) {
				tm->tm_hour = 23;
			}
			if (str_len < 14) {
				tm->tm_min = 59;
				tm->tm_sec = 59;
			}
			if (str_len < 17) {
				tm->tm_sec = 59;
			}
			/* round fsec up .555 to .555999 */
			/* this is not strict algorytm so if user enter .500 we will lose 00 */
			/* better to analyze initial string */
			if(fsec == 0)
				fsec_up = 999999;
			else
			{
				temp = fsec;
				while(temp > 0)
				{
					if(temp%10 == 0)
					{
						temp = temp/10;
						fsec_up += 9 * count; 
						count= count * 10;
					}
					else
						break;
				}
			}
			if (tm2timestamp(tm, (fsec + fsec_up), &tz, &max_date) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
						 errmsg("timestamp out of range")));

			return max_date;
		}
		else
			elog(ERROR, "expected min or max value");
	}
	return 0;
}

static void
reduce_timestamptz(void *acc, JsonbValue *val)
{
	DateAccumulator *dacc = acc;
	/* elog(INFO, "extract as date %s as %s", jsonbv_to_string(NULL, val), dacc->element_type);   */

	if(val != NULL && val->type == jbvString)
	{

		Datum date = date_bound(val->val.string.val, val->val.string.len, dacc->minmax);

		if(dacc->minmax == min)
		{
			if(dacc->acc != 0)
			{
				int gt = DirectFunctionCall2(timestamptz_cmp_timestamp, date, dacc->acc);
				/* elog(INFO, "compare %d", gt); */
				if(gt < 0)
					dacc->acc = date;
			}
			else if (date != 0)
			{
				dacc->acc = date;
			}
		}
		else if (dacc->minmax == max )
		{
			if(dacc->acc != 0)
			{
				int gt = DirectFunctionCall2(timestamptz_cmp_timestamp, date, dacc->acc);
				if(gt > 0)
					dacc->acc = date;
			}
			else if (date != 0)
			{
				dacc->acc = date;
			}

		}
		else
			elog(ERROR, "expected min or max value");
	}
}

PG_FUNCTION_INFO_V1(knife_extract_max_timestamptz);
Datum
knife_extract_max_timestamptz(PG_FUNCTION_ARGS)
{
	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	DateAccumulator acc;
	acc.minmax = max;
	acc.acc = 0;

	reduce_paths(value, paths, &acc, reduce_timestamptz);

	if (acc.acc != 0)
		PG_RETURN_NUMERIC(acc.acc);
	else
		PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(knife_extract_min_timestamptz);
Datum
knife_extract_min_timestamptz(PG_FUNCTION_ARGS)
{
	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	DateAccumulator acc;
	acc.minmax = min;
	acc.acc = 0;

	reduce_paths(value, paths, &acc, reduce_timestamptz);

	if (acc.acc != 0)
		PG_RETURN_NUMERIC(acc.acc);
	else
		PG_RETURN_NULL();
}

static
void reduce_timestamptz_array(void *acc, JsonbValue *val)
{
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;

  /* elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */

	if( val != NULL && jsonbv_type(val) == JVString )
		tacc->acc = accumArrayResult(tacc->acc,
									 (Datum) date_bound(val->val.string.val, val->val.string.len, min),
									 false,
									 TIMESTAMPTZOID,
									 CurrentMemoryContext);
}

PG_FUNCTION_INFO_V1(knife_extract_timestamptz);
Datum
knife_extract_timestamptz(PG_FUNCTION_ARGS)
{
	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	long num_results;

	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	num_results = reduce_paths(value, paths, &acc, reduce_timestamptz_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}

MinMax
minmax_from_string(char *s)
{
	if(strcmp(s, "min") == 0)
		return min;
	else if (strcmp(s, "max") == 0)
		return max;
	else
		elog(ERROR, "expected min or max");
}

PG_FUNCTION_INFO_V1(knife_date_bound);
Datum
knife_date_bound(PG_FUNCTION_ARGS)
{
	char       *date = text_to_cstring(PG_GETARG_TEXT_P(0));
	MinMax minmax = minmax_from_string(text_to_cstring(PG_GETARG_TEXT_P(1))); 

	Datum res = date_bound(date, strlen(date), minmax);

	if(res != 0)
		return res;
	else
		PG_RETURN_NULL();
}

