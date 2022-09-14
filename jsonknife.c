#include "postgres.h"
#include "miscadmin.h"
#include "fmgr.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "utils/formatting.h"
#include "utils/timestamp.h"
#include "utils/datetime.h"
#include "utils/json.h"
#include "catalog/pg_type.h"
#include "catalog/pg_collation.h"
#include "utils/jsonb.h"
#include "utils/varlena.h"

#ifndef PG_GETARG_JSONB
#define PG_GETARG_JSONB(x) DatumGetJsonbP(PG_GETARG_DATUM(x))
#endif

typedef void (*reduce_fn)(void *acc, JsonbValue *val);

typedef enum MinMax {min, max} MinMax;  

static MinMax minmax_from_string(char *s);

typedef enum JValueType {JVObject, JVArray, JVString, JVNumeric, JVBoolean, JVNull} JValueType; 

static Datum* date_bound(char *date_str, long str_len,  MinMax minmax);
static text *jsonbv_to_text(StringInfoData *out, JsonbValue *v);
static void initJsonbValue(JsonbValue *jbv, Jsonb *jb);
static bool knife_match(JsonbValue *value, JsonbValue *pattern);

typedef struct BasicAccumulator {
} BasicAccumulator;

typedef struct NumericAccumulator {
	MinMax minmax;
	Numeric acc;
} NumericAccumulator;

typedef struct ArrayAccumulator {
	ArrayBuildState *acc;
	bool case_insensitive;
} ArrayAccumulator;

typedef struct DateAccumulator {
	Datum*  acc;
	MinMax minmax;
} DateAccumulator;

typedef struct TextAccumulator {
	text *acc;
} TextAccumulator;

typedef struct BoolAccumulator {
	bool acc;
} BoolAccumulator;

static void update_numeric(NumericAccumulator *nacc, Numeric num);
/* static char * numeric_to_cstring(Numeric n); */

MinMax
minmax_from_string(char *s){
	if(strcmp(s, "min") == 0){
		return min;
	} else if (strcmp(s, "max") == 0) {
		return max;
	} else {
		elog(ERROR, "expected min or max");
	}
}

static JValueType
jsonbv_type(JsonbValue *v) {

  if(v == NULL){ return JVNull; }

  JsonbIterator *array_it;
  JsonbValue	array_value;
  int next_it;

  switch(v->type)
    {
    case jbvNull:
      return JVNull; 
      break;
    case jbvBool:
      return JVBoolean;
      break;
    case jbvString:
      return JVString;
      break;
    case jbvNumeric:
      return JVNumeric;
      break;
    case jbvBinary:
    case jbvArray:
    case jbvObject:
      {

        array_it = JsonbIteratorInit((JsonbContainer *) v->val.binary.data);
        next_it = JsonbIteratorNext(&array_it, &array_value, true);
        if(next_it == WJB_BEGIN_ARRAY){
          return JVArray;
        }
        else if(next_it == WJB_BEGIN_OBJECT){
          return JVObject;
        }
      }
      break;
    default:
      return JVNull;
      elog(ERROR, "Unknown jsonb type: %d", v->type);
    }
  return JVNull;
}

static inline StringInfoData *
append_jsonbv_to_buffer(StringInfoData *out, JsonbValue *v){
	if (out == NULL)
		out = makeStringInfo();

	switch(v->type)
	{
    case jbvNull:
		return NULL;
		break;
    case jbvBool:
		appendStringInfoString(out, (v->val.boolean ? "true" : "false"));
		break;
    case jbvString:
		appendBinaryStringInfo(out, v->val.string.val, v->val.string.len);
		break;
    case jbvNumeric:
		appendStringInfoString(out, DatumGetCString(DirectFunctionCall1(numeric_out, PointerGetDatum(v->val.numeric))));
		break;
    case jbvBinary:
    case jbvArray:
    case jbvObject:
	{
		Jsonb *binary = JsonbValueToJsonb(v);
        (void) JsonbToCString(out, &binary->root, -1);
	}
	break;
    default:
		elog(ERROR, "Wrong jsonb type: %d", v->type);
	}
	return out;

}
/* this function convert JsonbValue to string, */
/* StringInfoData out buffer is optional */
static inline char *
jsonbv_to_string(StringInfoData *out, JsonbValue *v){
	if (out == NULL)
		out = makeStringInfo();
	append_jsonbv_to_buffer(out, v);
	return out->data;
}

text *jsonbv_to_text(StringInfoData *out, JsonbValue *v){
	if (v == NULL) 
		return NULL;

	if (out == NULL)
		out = makeStringInfo();

	appendStringInfoSpaces(out, VARHDRSZ);
	append_jsonbv_to_buffer(out, v);
	SET_VARSIZE(out->data, out->len);
	return (text *)out->data;
}

void initJsonbValue(JsonbValue *jbv, Jsonb *jb) {
	jbv->type = jbvBinary;
	jbv->val.binary.data = &jb->root;
	jbv->val.binary.len = VARSIZE_ANY_EXHDR(jb);
}

/* return value of obj key */
static inline JsonbValue *
jsonb_get_key(JsonbValue *obj, JsonbValue *key){
	if(obj->type == jbvBinary){
		return findJsonbValueFromContainer((JsonbContainer *) obj->val.binary.data , JB_FOBJECT, key);
	} else {
		return NULL;
	}
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
				return DatumGetInt32(DirectFunctionCall2(numeric_cmp,
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

bool knife_match(JsonbValue *value, JsonbValue *pattern){
  if(value == NULL || pattern == NULL) {
    return false;
  }

  JValueType vtype = jsonbv_type(value);
  JValueType ptype = jsonbv_type(pattern);

  if((vtype == JVString || vtype == JVNumeric || vtype == JVBoolean) && (ptype == JVString || ptype == JVNumeric || ptype == JVBoolean)){

    /* elog(INFO, "compare prim %d, %s with %s", */
    /*      compareJsonbScalarValue(value, pattern), */
    /*      jsonbv_to_string(NULL, value), */
    /*      jsonbv_to_string(NULL, pattern)); */

    if(compareJsonbScalarValue(value, pattern) == 0) {
      return true;
    } else {
      return false;
    }
  }

  if( vtype == JVObject && ptype == JVObject){
    /* elog(INFO, "compare objects"); */

    JsonbIterator *patt_it;
    patt_it = JsonbIteratorInit((JsonbContainer *) pattern->val.binary.data);

    JsonbIterator *val_it;
    val_it = JsonbIteratorInit((JsonbContainer *) value->val.binary.data);

	bool res = JsonbDeepContains(&val_it, &patt_it);

    /* elog(INFO, "MATCH %d, %s with %s", */
	/* 	 res, */
    /*      jsonbv_to_string(NULL, value), */
    /*      jsonbv_to_string(NULL, pattern)); */

	return res;

    /* /\* JsonbValue *path_item; *\/ */
    /* JsonbValue	array_value; */
    /* JsonbValue *sample_value = NULL; */
    /* int next_it; */
    /* bool matched = true; */

    /* array_it = JsonbIteratorInit((JsonbContainer *) pattern->val.binary.data); */
    /* next_it = JsonbIteratorNext(&array_it, &array_value, true); */

    /* while (matched == true && (next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE){ */
    /*   if (next_it == WJB_KEY){ */
    /*     elog(INFO, "compare keys"); */
    /*     sample_value = jsonb_get_key(value, &array_value); */
    /*     if(sample_value == NULL){ */
    /*       matched = false; */
    /*     } */
    /*   } else if ( next_it == WJB_VALUE ) { */
    /*     elog(INFO, "compare vals %s: %s", sample_value, &array_value); */
    /*     if(! knife_match(sample_value, &array_value)){ */
    /*       matched = false; */
    /*     } */
    /*   } */
    /* } */
    /* elog(INFO, "matched %d", matched); */
    /* return matched; */
  }
  return false;
}

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

  /* elog(INFO, "enter with: %s ", jsonbv_to_string(NULL, jbv)); */

  if(jbv == NULL) { return 0; }

  if( current_idx < path_len ){
    path_item = path[current_idx];
  }

  if(jsonbv_type(jbv) == JVArray && ( path_item == NULL || jsonbv_type(path_item) != JVNumeric )){

    array_it = JsonbIteratorInit((JsonbContainer *) jbv->val.binary.data);
    next_it = JsonbIteratorNext(&array_it, &array_value, true);

    while ((next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE){
      if(next_it == WJB_ELEM){
        num_results += reduce_path(&array_value, path, current_idx, path_len, acc, fn);
      }
    }
    return num_results;
  }

  if(current_idx == path_len && jbv != NULL){
    fn(acc, jbv);
    return 1;
  }


  switch(jsonbv_type(path_item)) {
  case JVString:
    /* elog(INFO, " in key: %s", jsonbv_to_string(NULL, path_item)); */
    if(jsonbv_type(jbv) == JVObject){
      /* elog(INFO, " * in object: %s", jsonbv_to_string(NULL, jbv)); */
      next_v = jsonb_get_key(jbv, path_item);
      if(next_v != NULL){
        num_results += reduce_path(next_v, path, (current_idx + 1), path_len, acc, fn);
      }
    }
    break;
  case JVNumeric:
    /* elog(INFO, " in index: %s", jsonbv_to_string(NULL, path_item)); */
    if(jsonbv_type(jbv) == JVArray) {

      array_it = JsonbIteratorInit((JsonbContainer *) jbv->val.binary.data);
      next_it = JsonbIteratorNext(&array_it, &array_value, true);
      int array_index = 0;
      int required_index = DatumGetInt32(DirectFunctionCall1(numeric_int4, NumericGetDatum(path_item->val.numeric)));
      /* elog(INFO, "looking for index %d", required_index); */
      while ((next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE && array_index != -1){
        if(next_it == WJB_ELEM){
			/* elog(INFO, " %d = %d", array_index, required_index); */
          if(array_index == required_index) {
            num_results += reduce_path(&array_value, path, (current_idx +1), path_len, acc, fn);
			break;
          }
		  array_index++;
        }
      }
    }
    break;
  case JVObject:
    if(jsonbv_type(jbv) == JVObject){
      if(knife_match(jbv, path_item)){
        /* elog(INFO, "matched"); */
        num_results += reduce_path(jbv, path, (current_idx + 1), path_len, acc, fn);
      }
    }
    break;
  case JVArray:
  case JVBoolean:
  case JVNull:
    break;
  }

  return num_results;
}

/* static */
/* void reduce_debug(void *acc, JsonbValue *val){ */
/*   elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */
/* } */

static
void reduce_jsonb_array(void *acc, JsonbValue *val){
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;

  /* elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */

	if( val != NULL ) {
		tacc->acc = accumArrayResult(tacc->acc, (Datum) JsonbValueToJsonb(val), false, JSONBOID, CurrentMemoryContext);
	}
}


static
int to_json_path(JsonbValue *arr, JsonbValue **pathArr) {
	int path_len = 0;
	JsonbIterator *iter;
	JsonbValue	*item;
	int next_it;

	if (arr != NULL && arr->type == jbvBinary){

		item = palloc(sizeof(JsonbValue));
		iter = JsonbIteratorInit((JsonbContainer *) arr->val.binary.data);
		next_it = JsonbIteratorNext(&iter, item, true);

		if(next_it == WJB_BEGIN_ARRAY){
			while ((next_it = JsonbIteratorNext(&iter, item, true)) != WJB_DONE){
				/* elog(INFO, "next: %s", jsonbv_to_string(NULL, item)); */
				if(next_it == WJB_ELEM){
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
reduce_paths(Jsonb *value, Jsonb *paths, void *acc, reduce_fn fn) {

  JsonbValue jdoc;
  initJsonbValue(&jdoc, value);

  JsonbValue jpaths;
  initJsonbValue(&jpaths, paths);

  JsonbIterator *path_iter;
  JsonbValue	path_item;
  int next_it;

	JsonbValue *pathArr[100];
	int path_len;

  long num_results = 0;


    if (jpaths.type == jbvBinary) {

      path_iter = JsonbIteratorInit((JsonbContainer *) jpaths.val.binary.data);

      next_it = JsonbIteratorNext(&path_iter, &path_item, true);

      if(next_it == WJB_BEGIN_ARRAY){
        while ((next_it = JsonbIteratorNext(&path_iter, &path_item, true)) != WJB_DONE){
          if(next_it == WJB_ELEM){
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
knife_extract(PG_FUNCTION_ARGS) {

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	long num_results = reduce_paths(value, paths, &acc, reduce_jsonb_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}

static
void reduce_text_array(void *acc, JsonbValue *val){
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;

  /* elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */

	if( val != NULL && jsonbv_type(val) == JVString ) {
		tacc->acc = accumArrayResult(tacc->acc,
                                 (Datum) jsonbv_to_text(NULL, val),
                                 false, TEXTOID, CurrentMemoryContext);
	}
}


PG_FUNCTION_INFO_V1(knife_extract_text);
Datum
knife_extract_text(PG_FUNCTION_ARGS) {

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	long num_results = reduce_paths(value, paths, &acc, reduce_text_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}

static
void update_numeric(NumericAccumulator *nacc, Numeric num){
	if(nacc->acc == NULL){
		nacc->acc = num;
	} else {

		bool gt = DirectFunctionCall2(numeric_gt, (Datum) nacc->acc, (Datum) num);
		/* elog(INFO, "%s > %s, gt %d min %d", numeric_to_cstring(num), numeric_to_cstring(nacc->acc), gt, nacc->minmax); */
		if (nacc->minmax == min && gt == 1){
			nacc->acc = num;
		} else if (nacc->minmax == max && gt == 0){
			nacc->acc = num;
		}
	}
}

static
void reduce_numeric(void *acc, JsonbValue *val){

	NumericAccumulator *nacc = acc;

	/* elog(INFO, "extract as number [%s] %s", nacc->element_type, jsonbv_to_string(NULL, val)); */

	if( val == NULL || val->type == jbvNull) {return;}

	if( val->type == jbvNumeric){

		update_numeric(nacc, val->val.numeric);

	} else if( val->type == jbvString){

		long len =val->val.string.len;
    char *num_str = palloc( len+ 1);
		memcpy(num_str, val->val.string.val, len);
		num_str[len] = '\0';

		update_numeric(nacc, DatumGetNumeric(DirectFunctionCall3(numeric_in, CStringGetDatum(num_str), 0, -1)));
	} else {
		elog(ERROR, "Could not extract as number %s", jsonbv_to_string(NULL, val));
	}
}

PG_FUNCTION_INFO_V1(knife_extract_max_numeric);
Datum
knife_extract_max_numeric(PG_FUNCTION_ARGS) {

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

PG_FUNCTION_INFO_V1(knife_extract_min_numeric);
Datum
knife_extract_min_numeric(PG_FUNCTION_ARGS) {

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

static
void reduce_numeric_array(void *acc, JsonbValue *val){
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;

  /* elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */

	if( val != NULL && jsonbv_type(val) == JVNumeric ) {
		tacc->acc = accumArrayResult(tacc->acc,
                                 (Datum) val->val.numeric,
                                 false, NUMERICOID, CurrentMemoryContext);
	}
}

PG_FUNCTION_INFO_V1(knife_extract_numeric);
Datum
knife_extract_numeric(PG_FUNCTION_ARGS) {

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	long num_results = reduce_paths(value, paths, &acc, reduce_numeric_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}

Datum*
date_bound(char *date_str, long str_len,  MinMax minmax){
	if(date_str != NULL) {
		/* elog(INFO, "date_str: '%s', %d", date_str, str_len ); */

		char *ref_str = "0000-01-01T00:00:00";
		long ref_str_len = strlen(ref_str);

		long date_in_len = (str_len > ref_str_len) ? str_len : ref_str_len;
	    char *date_in = palloc(date_in_len + 1);
		memcpy(date_in, date_str, str_len);

		if(str_len > 18) {
			date_in[str_len] = '\0';
			Datum timestamp =  DirectFunctionCall3(timestamptz_in,
												   CStringGetDatum(date_in),
												   ObjectIdGetDatum(InvalidOid),
												   Int32GetDatum(-1));

			/* elog(INFO, "input_if: '%s', %ld", date_in, timestamp); */

			memcpy(date_in, &timestamp, SIZEOF_DATUM);
			return (Datum*)date_in;
		}

		if( str_len < ref_str_len){
			long diff = (ref_str_len - str_len);
			memcpy(date_in + str_len, ref_str + str_len, diff);
		}

		date_in[date_in_len] = '\0';




		Datum min_date = DirectFunctionCall3(timestamptz_in,
											 CStringGetDatum(date_in),
											 ObjectIdGetDatum(InvalidOid),
											 Int32GetDatum(-1));



		if(minmax == min) {
			/* elog(INFO, "input_min: '%s', %ld", date_in, min_date); */

			memcpy(date_in, &min_date, SIZEOF_DATUM);
			return (Datum*)date_in;
		} else if (minmax == max ) {
			Timestamp	max_date;
			int			tz;
			struct pg_tm tt, *tm = &tt;
			fsec_t		fsec;

			if (timestamp2tm(min_date, &tz, tm, &fsec, NULL, NULL) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
						 errmsg("timestamp out of range")));

			/* elog(INFO, "get tm %d y %d m %d d %d fsec", tm->tm_year, tm->tm_mon, tm->tm_mday, fsec); */

			if (str_len < 5) {
				tm->tm_mon = 12;
			}
			if (str_len < 8) {
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

			/* turn of this millisecond  suspicios transform */
			/* round fsec up .555 to .555999 */
			/* this is not strict algorytm so if user enter .500 we will lose 00 */
			/* better to analyze initial string */
			/* int fsec_up = 0, temp, count = 1; */
			/* if(fsec == 0){ */
			/* 	fsec_up = 999999; */
			/* } else { */
			/* 	temp = fsec; */
			/* 	while(temp > 0) { */
			/* 		if(temp%10 == 0) { */
			/* 			temp = temp/10; */
			/* 			fsec_up += 9 * count;  */
			/* 			count= count * 10; */
			/* 		} else { */
			/* 			break; */
			/* 		} */
			/* 	} */
			/* } */


			if (tm2timestamp(tm, fsec, &tz, &max_date) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE),
						 errmsg("timestamp out of range")));

			/* elog(INFO, "input_max: '%s', %ld", date_in, max_date); */
			Datum result_max = TimestampGetDatum(max_date);
			memcpy(date_in, &result_max, SIZEOF_DATUM);
			return (Datum*)date_in;
		} else {
			elog(ERROR, "expected min or max value");
		}
	} else {
		return NULL;
	}
}


static
void reduce_timestamptz(void *acc, JsonbValue *val){
	DateAccumulator *dacc = acc;
	/* elog(INFO, "extract as date %s as %s", jsonbv_to_string(NULL, val), dacc->element_type);   */

	if(val != NULL && val->type == jbvString) {

		Datum* datum = date_bound(val->val.string.val, val->val.string.len, dacc->minmax);
		if (datum == NULL) {
			return;
		}

		/* elog(INFO, "reduce_timestamptz %s %ld %ld", val->val.string.val, *datum); */

		if(dacc->minmax == min) {
			if(dacc->acc != NULL){
				int gt = DirectFunctionCall2(timestamptz_cmp_timestamp, *datum, *dacc->acc);
				/* elog(INFO, "compare %d", gt); */
				if(gt < 0) {
					dacc->acc = datum;
				}
			} else {
				dacc->acc = datum;
			}
		} else if (dacc->minmax == max ) {
			if(dacc->acc != NULL){
				int gt = DirectFunctionCall2(timestamptz_cmp_timestamp, *datum, *dacc->acc);
				if(gt > 0) {
					dacc->acc = datum;
				}
			} else {
				dacc->acc = datum;
			}
		} else {
			elog(ERROR, "expected min or max value");
		}
	}

}



PG_FUNCTION_INFO_V1(knife_extract_max_timestamptz);
Datum
knife_extract_max_timestamptz(PG_FUNCTION_ARGS) {

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	DateAccumulator acc;
	acc.minmax = max;
	acc.acc = NULL;

	reduce_paths(value, paths, &acc, reduce_timestamptz);

	if (acc.acc == NULL) {
		PG_RETURN_NULL();
	} else {
		/* elog(INFO, "knife_extract_max_timestamptz %ld", result); */

		PG_RETURN_NUMERIC(*acc.acc);
	}
}

PG_FUNCTION_INFO_V1(knife_extract_min_timestamptz);
Datum
knife_extract_min_timestamptz(PG_FUNCTION_ARGS) {

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	DateAccumulator acc;
	acc.minmax = min;
	acc.acc = NULL;

	reduce_paths(value, paths, &acc, reduce_timestamptz);

	if (acc.acc == NULL) {
		PG_RETURN_NULL();
	} else {
		/* elog(INFO, "knife_extract_min_timestamptz %ld", result); */

		PG_RETURN_NUMERIC(*acc.acc);
	}
}


static
void reduce_timestamptz_array(void *acc, JsonbValue *val){
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;

	/* elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */

	if( val != NULL && jsonbv_type(val) == JVString ) {
		Datum* datum = date_bound(val->val.string.val, val->val.string.len, min);
		if (datum == NULL) {
			tacc->acc = accumArrayResult(tacc->acc, 0, true, TIMESTAMPTZOID, CurrentMemoryContext);
		} else {
			tacc->acc = accumArrayResult(tacc->acc, *datum,
					false, TIMESTAMPTZOID, CurrentMemoryContext);
		}
	}
}

PG_FUNCTION_INFO_V1(knife_extract_timestamptz);
Datum
knife_extract_timestamptz(PG_FUNCTION_ARGS) {

	Jsonb      *value = PG_GETARG_JSONB(0);
	Jsonb      *paths = PG_GETARG_JSONB(1);

	ArrayAccumulator acc;
	acc.acc = NULL;
	acc.case_insensitive = false;

	long num_results = reduce_paths(value, paths, &acc, reduce_timestamptz_array);

	/* elog(INFO, "knife_extract_timestamptz num_result: %ld", num_results); */

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}



PG_FUNCTION_INFO_V1(knife_date_bound);

Datum
knife_date_bound(PG_FUNCTION_ARGS) {
	char       *date = text_to_cstring(PG_GETARG_TEXT_P(0));
	MinMax minmax = minmax_from_string(text_to_cstring(PG_GETARG_TEXT_P(1))); 

	Datum* datum = date_bound(date, strlen(date), minmax);
	if (datum == NULL) {
		PG_RETURN_NULL();
	}

	PG_RETURN_TIMESTAMP(*datum);
}
