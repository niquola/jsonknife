# Search crash test

## Run

``` bash
./test.sh > test.sql && psql -v ON_ERROR_STOP=1 root < test.sq
```


## Search Parameter Types

| Data Type        | number   | date     | string   | token    | reference | quantity | uri      |
|---|---|---|---|---|---|---|---|
| __Primitive__    | -------- | -------- | -------- | -------- | --------  | -------- | -------- |
| boolean          |          |          |          | Y        |           |          |          |
| code             |          |          |          | Y        |           |          |          |
| time             |          | Y        |          |          |           |          |          |
| date             |          | Y        |          |          |           |          |          |
| dateTime         |          | Y        |          |          |           |          |          |
| instant          |          | Y        |          |          |           |          |          |
| decimal          | Y        |          |          |          |           |          |          |
| integer          | Y        |          |          |          |           |          |          |
| _unsignedInt_    | Y        |          |          |          |           |          |          |
| _positiveInt_    | Y        |          |          |          |           |          |          |
| string           |          |          | Y        | Y        |           |          |          |
| _code_           |          |          | Y        |          |           |          |          |
| _id_             |          |          | Y        |          |           |          |          |
| _markdown_       |          |          | Y        |          |           |          |          |
| uri              |          |          |          |          | Y         |          | Y        |
| _oid_            |          |          |          |          |           |          | Y        |
| __Complex__      | -------- | -------- | -------- | -------- | --------  | -------- | -------- |
| Ratio            |          |          |          |          |           |          |          |
| Period           |          | Y        |          |          |           |          |          |
| Range            |          |          |          |          |           |          |          |
| Attachment       |          |          |          |          |           |          |          |
| Identifier       |          |          |          | Y        |           |          |          |
| Timing           |          | Y        |          |          |           |          |          |
| HumanName        |          |          | Y        |          |           |          |          |
| Coding           |          |          |          | Y        |           |          |          |
| Annotation       |          |          |          |          |           |          |          |
| Signature        |          |          |          |          |           |          |          |
| Address          |          |          | Y        |          |           |          |          |
| CodeableConcept  |          |          |          | Y        |           |          |          |
| Quantity         | Y        |          |          |          |           | Y        |          |
| _Age_            | Y        |          |          |          |           | Y        |          |
| _Distance_       | Y        |          |          |          |           | Y        |          |
| _SimpleQuantity_ | Y        |          |          |          |           | Y        |          |
| _Duration_       | Y        |          |          |          |           | Y        |          |
| _Count_          | Y        |          |          |          |           | Y        |          |
| _Money_          | Y        |          |          |          |           | Y        |          |
| SampledData      |          |          |          |          |           |          |          |
| ContactPoint     |          |          |          | Y        |           |          |          |
| __Other__        | -------- | -------- | -------- | -------- | --------  | -------- | -------- |
| Reference        |          |          |          |          | Y         |          |          |
| Narrative        |          |          |          |          |           |          |          |
| Extension        |          |          |          |          |           |          |          |




## Crash test

``` bash
./crash.sh > crash.sql && psql root < crash.sql
```
