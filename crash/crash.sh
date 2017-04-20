#!/bin/bash
set -e

RESOURCE=$(<resource.json)

# Search types from doc https://www.hl7.org/fhir/search.html
SEARCH_TYPES=(date number reference string token uri quantity)

# Supported search types
# SEARCH_TYPES=(date number reference string token )

# Data types https://www.hl7.org/FHIR/datatypes.html
PRIMITIVE_DATA_TYPES=(integer decimal unsignedInt positiveInt instant time date dateTime boolean string code id markdown uri oid base64Binary)

COMPLEX_DATA_TYPES=(Ratio SampledData Period Quantity Age Distance SimpleQuantity Duration Count Money Attachment Range Coding CodeableConcept HumanName Address ContactPoint Identifier Timing Signature Annotation)

DATA_TYPES=("${PRIMITIVE_DATA_TYPES[@]}" "${COMPLEX_DATA_TYPES[@]}")



PATHS=(._empty_obj ._empty_str ._empty_arr .undefined)
for p in "${SEARCH_TYPES[@]}"
do
	PATHS+=(".$p.value")
	PATHS+=(".$p.array")
done


function mk_query() {
	local SEARCH_TYPE=$1
	local PTH=$2
  local	DATA_TYPE=$3
	local MODIFICATION=""

	if [ "$SEARCH_TYPE" == number ] || [ "$SEARCH_TYPE" == date ]
	then
		MODIFICATION=", 'max'"
	fi

  echo "SELECT fhirpath_as_$SEARCH_TYPE(:resource, '$PTH', '$DATA_TYPE' $MODIFICATION);"
}

echo "select '''$RESOURCE''' resource \\gset"

for st in "${SEARCH_TYPES[@]}"
do
	for dt in "${DATA_TYPES[@]}"
	do
		for p in "${PATHS[@]}"
		do
			SQL=$(mk_query $st $p $dt)
			echo $SQL
		done
	done
done
