#include <stdio.h>
#include <string.h>
#include "sbf.h"

int main(void) {
    SbfSchema schema = {0};
    schema.field_count = 2;
    schema.fields[0].type = SBF_TYPE_STRING;
    schema.fields[0].name_len = (uint8_t)strlen("id");
    memcpy(schema.fields[0].name, "id", schema.fields[0].name_len);
    schema.fields[1].type = SBF_TYPE_INT32;
    schema.fields[1].name_len = (uint8_t)strlen("age");
    memcpy(schema.fields[1].name, "age", schema.fields[1].name_len);

    SbfRecord recs[2];
    SbfValue v0[2] = { { .int_val = 0, .str_val = "abc" }, { .int_val = 30, .str_val = NULL } };
    SbfValue v1[2] = { { .int_val = 0, .str_val = "xyz" }, { .int_val = 25, .str_val = NULL } };
    recs[0].values = v0;
    recs[1].values = v1;

    int rc = sbf_save("sample.sbf", &schema, recs, 2);
    printf("sbf_save => %d\n", rc);
    return 0;
}
