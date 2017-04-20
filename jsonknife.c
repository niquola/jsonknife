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
jsonb_get_key(char *key, JsonbValue *obj){

	JsonbValue	key_v;
	key_v.type = jbvString;
	key_v.val.string.len = strlen(key);
	key_v.val.string.val = key;

	/* test it's container (object) */
	if(obj->type == jbvBinary){
		/* we need to use special function to get valid JsonbValue */
		return findJsonbValueFromContainer((JsonbContainer *) obj->val.binary.data , JB_FOBJECT, &key_v);
	} else {
		return NULL;
	}
}


PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(knife_extract);
Datum
knife_extract(PG_FUNCTION_ARGS) {

	Jsonb      *jb = PG_GETARG_JSONB(0);
	Jsonb      *path = PG_GETARG_JSONB(1);

	JsonbValue jpath;
	initJsonbValue(&jpath, path);

	JsonbValue *arr = &jpath;

	elog(INFO, "reduce array %s", jsonbv_to_string(NULL, arr));

	JsonbIterator *iter;
	JsonbValue	item;
	int next_it;

	if (arr != NULL && arr->type == jbvBinary){

		iter = JsonbIteratorInit((JsonbContainer *) arr->val.binary.data);
		next_it = JsonbIteratorNext(&iter, &item, true);

		if(next_it == WJB_BEGIN_ARRAY){
			while ((next_it = JsonbIteratorNext(&iter, &item, true)) != WJB_DONE){
				if(next_it == WJB_ELEM){

					elog(INFO, "item %s", jsonbv_to_string(NULL, &item));

				}
			}
		}
	}


	PG_RETURN_POINTER(path);
}
