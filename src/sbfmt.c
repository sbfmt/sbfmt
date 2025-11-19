#define _POSIX_C_SOURCE 200809L
#include "sbf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Internal helpers for little-endian writes/reads */
static int write_u16_le(FILE *f, uint16_t v) {
    uint8_t b[2] = { (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF) };
    return fwrite(b, 1, 2, f) == 2 ? SBF_OK : SBF_EIO;
}

static int write_u32_le(FILE *f, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v & 0xFF),
        (uint8_t)((v >> 8) & 0xFF),
        (uint8_t)((v >> 16) & 0xFF),
        (uint8_t)((v >> 24) & 0xFF)
    };
    return fwrite(b, 1, 4, f) == 4 ? SBF_OK : SBF_EIO;
}

static int read_u16_le(FILE *f, uint16_t *out) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return SBF_EIO;
    *out = (uint16_t)(b[0] | (b[1] << 8));
    return SBF_OK;
}

static int read_u32_le(FILE *f, uint32_t *out) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return SBF_EIO;
    *out = (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
    return SBF_OK;
}

/* Constants */
static const char sbf_magic[4] = { 'S', 'B', 'F', 0 }; /* trailing NUL optional */

/* Save implementation (basic, no heavy validation) */
int sbf_save(const char *path, const SbfSchema *schema, const SbfRecord *records, int record_count) {
    if (!path || !schema) return SBF_EINVALIDARG;
    FILE *f = fopen(path, "wb");
    if (!f) return SBF_EIO;

    /* Header: magic (4), version (u16), field_count (u16) */
    if (fwrite(sbf_magic, 1, 4, f) != 4) { fclose(f); return SBF_EIO; }
    if (write_u16_le(f, (uint16_t)SBF_VERSION_MAJOR) != SBF_OK) { fclose(f); return SBF_EIO; }
    if (write_u16_le(f, (uint16_t)schema->field_count) != SBF_OK) { fclose(f); return SBF_EIO; }

    /* Schema entries */
    for (uint16_t i = 0; i < schema->field_count; ++i) {
        const SbfField *fld = &schema->fields[i];
        if (fwrite(&fld->type, 1, 1, f) != 1) { fclose(f); return SBF_EIO; }
        uint8_t nl = fld->name_len;
        if (fwrite(&nl, 1, 1, f) != 1) { fclose(f); return SBF_EIO; }
        if (nl > 0) {
            if (fwrite(fld->name, 1, nl, f) != nl) { fclose(f); return SBF_EIO; }
        }
    }

    /* Records */
    for (int r = 0; r < record_count; ++r) {
        const SbfRecord *rec = &records[r];
        for (uint16_t fi = 0; fi < schema->field_count; ++fi) {
            uint8_t typ = schema->fields[fi].type;
            if (typ == SBF_TYPE_STRING) {
                const char *s = rec->values[fi].str_val ? rec->values[fi].str_val : "";
                size_t len = strlen(s);
                if (len > 0xFFFF) { fclose(f); return SBF_EFORMAT; }
                if (write_u16_le(f, (uint16_t)len) != SBF_OK) { fclose(f); return SBF_EIO; }
                if (len > 0 && fwrite(s, 1, len, f) != len) { fclose(f); return SBF_EIO; }
            } else if (typ == SBF_TYPE_INT32) {
                if (write_u32_le(f, rec->values[fi].int_val) != SBF_OK) { fclose(f); return SBF_EIO; }
            } else {
                fclose(f);
                return SBF_EFORMAT;
            }
        }
    }

    if (fclose(f) != 0) return SBF_EIO;
    return SBF_OK;
}

/* Load - simple implementation: reads entire file into records (allocates memory). */
int sbf_load(const char *path, SbfSchema *schema_out, SbfRecord **records_out, int *record_count_out) {
    if (!path || !schema_out || !records_out || !record_count_out) return SBF_EINVALIDARG;
    FILE *f = fopen(path, "rb");
    if (!f) return SBF_EIO;

    char magic[4];
    if (fread(magic, 1, 4, f) != 4) { fclose(f); return SBF_EFORMAT; }
    if (memcmp(magic, sbf_magic, 4) != 0) { fclose(f); return SBF_EFORMAT; }

    uint16_t version;
    if (read_u16_le(f, &version) != SBF_OK) { fclose(f); return SBF_EIO; }
    if ((int)version != SBF_VERSION_MAJOR) { fclose(f); return SBF_EVERSION; }

    uint16_t field_count;
    if (read_u16_le(f, &field_count) != SBF_OK) { fclose(f); return SBF_EIO; }
    if (field_count == 0 || field_count > SBF_MAX_FIELDS) { fclose(f); return SBF_EFORMAT; }

    /* Parse schema */
    schema_out->field_count = field_count;
    for (uint16_t i = 0; i < field_count; ++i) {
        uint8_t type;
        uint8_t name_len;
        if (fread(&type, 1, 1, f) != 1) { fclose(f); return SBF_EIO; }
        if (fread(&name_len, 1, 1, f) != 1) { fclose(f); return SBF_EIO; }
        if (name_len > SBF_MAX_FIELD_NAME) { fclose(f); return SBF_EFORMAT; }
        if (name_len > 0) {
            if (fread(schema_out->fields[i].name, 1, name_len, f) != name_len) { fclose(f); return SBF_EIO; }
        }
        schema_out->fields[i].name[name_len] = '\0';
        schema_out->fields[i].name_len = name_len;
        schema_out->fields[i].type = type;
    }

    /* Naive approach: read records into a dynamic array by scanning until EOF */
    /* We will do two-pass: first count records by scanning, then allocate & re-read. */

    long data_start = ftell(f);
    if (data_start < 0) { fclose(f); return SBF_EIO; }

    /* Count records */
    int count = 0;
    while (1) {
        int ok = 1;
        for (uint16_t i = 0; i < field_count; ++i) {
            uint8_t typ = schema_out->fields[i].type;
            if (typ == SBF_TYPE_STRING) {
                uint16_t len;
                if (read_u16_le(f, &len) != SBF_OK) { ok = 0; break; }
                if (fseek(f, len, SEEK_CUR) != 0) { ok = 0; break; }
            } else if (typ == SBF_TYPE_INT32) {
                uint32_t tmp;
                if (read_u32_le(f, &tmp) != SBF_OK) { ok = 0; break; }
            } else {
                ok = 0; break;
            }
        }
        if (!ok) break;
        ++count;
    }

    /* If no complete record found, allow zero records. Reset to data start. */
    if (fseek(f, data_start, SEEK_SET) != 0) { fclose(f); return SBF_EIO; }

    /* Allocate records */
    SbfRecord *records = calloc((size_t)count, sizeof(SbfRecord));
    if (!records) { fclose(f); return SBF_EMEM; }

    for (int r = 0; r < count; ++r) {
        records[r].values = calloc(field_count, sizeof(SbfValue));
        if (!records[r].values) { /* cleanup */ sbf_free_records(records, r, field_count); fclose(f); return SBF_EMEM; }
        for (uint16_t i = 0; i < field_count; ++i) {
            uint8_t typ = schema_out->fields[i].type;
            if (typ == SBF_TYPE_STRING) {
                uint16_t len;
                if (read_u16_le(f, &len) != SBF_OK) { sbf_free_records(records, r+1, field_count); fclose(f); return SBF_EIO; }
                char *buf = malloc((size_t)len + 1);
                if (!buf) { sbf_free_records(records, r+1, field_count); fclose(f); return SBF_EMEM; }
                if (len > 0) {
                    if (fread(buf, 1, len, f) != len) { free(buf); sbf_free_records(records, r+1, field_count); fclose(f); return SBF_EIO; }
                }
                buf[len] = '\0';
                records[r].values[i].str_val = buf;
            } else if (typ == SBF_TYPE_INT32) {
                uint32_t v;
                if (read_u32_le(f, &v) != SBF_OK) { sbf_free_records(records, r+1, field_count); fclose(f); return SBF_EIO; }
                records[r].values[i].int_val = v;
            } else {
                sbf_free_records(records, r+1, field_count);
                fclose(f);
                return SBF_EFORMAT;
            }
        }
    }

    fclose(f);
    *records_out = records;
    *record_count_out = count;
    return SBF_OK;
}

void sbf_free_records(SbfRecord *records, int record_count, int field_count) {
    if (!records) return;
    for (int r = 0; r < record_count; ++r) {
        if (records[r].values) {
            for (int i = 0; i < field_count; ++i) {
                if (records[r].values[i].str_val) {
                    free(records[r].values[i].str_val);
                }
            }
            free(records[r].values);
        }
    }
    free(records);
}

/* Lightweight header-inspector */
int sbf_inspect_header(const char *path, uint16_t *version_out, uint16_t *field_count_out) {
    if (!path || !version_out || !field_count_out) return SBF_EINVALIDARG;
    FILE *f = fopen(path, "rb");
    if (!f) return SBF_EIO;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4) { fclose(f); return SBF_EFORMAT; }
    if (memcmp(magic, sbf_magic, 4) != 0) { fclose(f); return SBF_EFORMAT; }
    if (read_u16_le(f, version_out) != SBF_OK) { fclose(f); return SBF_EIO; }
    if (read_u16_le(f, field_count_out) != SBF_OK) { fclose(f); return SBF_EIO; }
    fclose(f);
    return SBF_OK;
}

/* Streaming API stubs (left unimplemented; optional) */
struct SbfReader { FILE *f; SbfSchema schema; int eof; };
SbfReader *sbf_reader_open(const char *path) { (void)path; return NULL; }
int sbf_reader_next(SbfReader *r, SbfRecord *out) { (void)r; (void)out; return SBF_EFORMAT; }
void sbf_reader_close(SbfReader *r) { (void)r; }
