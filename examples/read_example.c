#include <stdio.h>
#include <stdlib.h>
#include "sbf.h"

int main(void) {
    SbfSchema schema;
    SbfRecord *records;
    int count;
    int rc = sbf_load("sample.sbf", &schema, &records, &count);
    if (rc != SBF_OK) {
        fprintf(stderr, "sbf_load failed: %d\n", rc);
        return 1;
    }
    printf("fields = %d, records = %d\n", schema.field_count, count);
    for (int r = 0; r < count; ++r) {
        printf("Record %d:\n", r);
        for (int i = 0; i < schema.field_count; ++i) {
            if (schema.fields[i].type == SBF_TYPE_STRING) {
                printf("  %s = %s\n", schema.fields[i].name, records[r].values[i].str_val ?: "(null)");
            } else {
                printf("  %s = %u\n", schema.fields[i].name, records[r].values[i].int_val);
            }
        }
    }
    sbf_free_records(records, count, schema.field_count);
    return 0;
}
