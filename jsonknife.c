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


typedef void (*reduce_fn)(void *acc, JsonbValue *val);

typedef enum MinMax {min, max} MinMax;  

static MinMax minmax_from_string(char *s);

typedef enum FPSearchType {FPToken, FPString, FPDate, FPNumeric, FPReference, FPTextSort, FPBool} FPSearchType;  
typedef enum JValueType {JVObject, JVArray, JVString, JVNumeric, JVBoolean, JVNull} JValueType; 

static Datum date_bound(char *date_str, long str_len,  MinMax minmax);

typedef struct BasicAccumulator {
	FPSearchType search_type;
} BasicAccumulator;

typedef struct NumericAccumulator {
	FPSearchType search_type;
	JsonbValue *json_path[100];
	MinMax minmax;
} NumericAccumulator;

typedef struct StringAccumulator {
	FPSearchType search_type;
	StringInfoData *buf;
} StringAccumulator;

typedef struct ArrayAccumulator {
	FPSearchType search_type;
	ArrayBuildState *acc;
	bool case_insensitive;
} ArrayAccumulator;

typedef struct DateAccumulator {
	FPSearchType search_type;
	Datum  acc;
	MinMax minmax;
} DateAccumulator;

typedef struct TextAccumulator {
	FPSearchType search_type;
	text *acc;
} TextAccumulator;

typedef struct BoolAccumulator {
	FPSearchType search_type;
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


  if(v == NULL){
    return JVNull;
  }

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

void
initJsonbValue(JsonbValue *jbv, Jsonb *jb) {
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

bool
knife_match(JsonbValue *value, JsonbValue *pattern){
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

    JsonbValue *path_item;
    JsonbIterator *array_it;
    JsonbValue	array_value;
    JsonbValue *sample_value = NULL;
    int next_it;
    bool matched = true;

    array_it = JsonbIteratorInit((JsonbContainer *) pattern->val.binary.data);
    next_it = JsonbIteratorNext(&array_it, &array_value, true);

    while (matched == true && (next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE){
      if (next_it == WJB_KEY){
        /* elog(INFO, "compare keys"); */
        sample_value = jsonb_get_key(value, &array_value);
        if(sample_value == NULL){
          matched = false;
        }
      } else if ( next_it == WJB_VALUE ) {
        /* elog(INFO, "compare vals %s: %s", sample_value, &array_value); */
        if(! knife_match(sample_value, &array_value)){
          matched = false;
        }
      }
    }
    /* elog(INFO, "matched %d", matched); */
    return matched;
  }
}

static long
reduce_path(JsonbValue *jbv, JsonbValue **path, int current_idx, int path_len, void *acc, reduce_fn fn)
{

  JsonbValue *next_v = NULL;
  JsonbValue *path_item;
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
    switch(jsonbv_type(jbv)) {
    case JVObject:
      /* elog(INFO, " * in object: %s", jsonbv_to_string(NULL, jbv)); */
      next_v = jsonb_get_key(jbv, path_item);
      if(next_v != NULL){
        num_results += reduce_path(next_v, path, (current_idx + 1), path_len, acc, fn);
      }
      break;
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
          if(array_index == required_index) {
            num_results += reduce_path(&array_value, path, (current_idx +1), path_len, acc, fn);
            array_index = -1;
          }
        }
        array_index++;
      }
    }
    break;
  case JVObject:

    switch(jsonbv_type(jbv)) {
    case JVObject:
      /* elog(INFO, "object"); */
      if(knife_match(jbv, path_item)){
        /* elog(INFO, "matched"); */
        num_results += reduce_path(jbv, path, (current_idx + 1), path_len, acc, fn);
      }
      break;

    default:
      elog(ERROR, "Wrong jsonb type: %d", path_item->type);
    }
  }

  return num_results;
}


void reduce_debug(void *acc, JsonbValue *val){
  elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val));
}

void reduce_jsonb_array(void *acc, JsonbValue *val){
	ArrayAccumulator *tacc = (ArrayAccumulator *) acc;

  /* elog(INFO, "leaf: %s", jsonbv_to_string(NULL, val)); */

	if( val != NULL ) {
		tacc->acc = accumArrayResult(tacc->acc, (Datum) JsonbValueToJsonb(val), false, JSONBOID, CurrentMemoryContext);
	}
}

int
to_json_path(Jsonb *path, JsonbValue **pathArr) {
	int path_len = 0;

	JsonbValue jpath;
	initJsonbValue(&jpath, path);
	JsonbValue *arr = &jpath;

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

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(knife_extract);
Datum
knife_extract(PG_FUNCTION_ARGS) {

	Jsonb      *jb = PG_GETARG_JSONB(0);
	Jsonb      *path = PG_GETARG_JSONB(1);

	JsonbValue jdoc;
	initJsonbValue(&jdoc, jb);

	JsonbValue *obj = &jdoc;

	JsonbValue *pathArr[100];
	int path_len = to_json_path(path, pathArr);

	ArrayAccumulator acc;
	acc.search_type = FPReference;
	acc.acc = NULL;
	acc.case_insensitive = false;

	long num_results = reduce_path(obj, pathArr, 0, path_len, &acc, reduce_jsonb_array);

	if (num_results > 0 && acc.acc != NULL)
		PG_RETURN_ARRAYTYPE_P(makeArrayResult(acc.acc, CurrentMemoryContext));
	else
		PG_RETURN_NULL();
}
