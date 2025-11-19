#ifndef SBF_H
#define SBF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library version */
#define SBF_VERSION_MAJOR 0
#define SBF_VERSION_MINOR 1

/* Visibility macro (for shared lib symbol export) */
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef SBF_SHARED
    #ifdef __GNUC__
      #define SBF_API __attribute__ ((dllexport))
    #else
      #define SBF_API __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define SBF_API __attribute__ ((dllimport))
    #else
      #define SBF_API
    #endif
  #endif
#else
  #if __GNUC__ >= 4
    #define SBF_API __attribute__ ((visibility ("default")))
  #else
    #define SBF_API
  #endif
#endif

/* Error codes */
enum {
    SBF_OK            = 0,
    SBF_EIO           = -1,
    SBF_EFORMAT       = -2,
    SBF_EMEM          = -3,
    SBF_EVERSION      = -4,
    SBF_EINVALIDARG   = -5,
};

/* Field types */
typedef enum {
    SBF_TYPE_STRING = 0,
    SBF_TYPE_INT32  = 1,
    /* future: FLOAT32, INT64, BYTES, etc. */
} SbfFieldType;

/* Maxs */
#define SBF_MAX_FIELDS 256
#define SBF_MAX_FIELD_NAME 255

/* Schema descriptor (public POD) */
typedef struct SbfField {
    uint8_t type;                  /* SbfFieldType */
    uint8_t name_len;              /* length of name */
    char    name[SBF_MAX_FIELD_NAME + 1];
} SbfField;

typedef struct {
    uint16_t field_count;
    SbfField fields[SBF_MAX_FIELDS];
} SbfSchema;

/* Record/value representation (simple, C-friendly) */
typedef struct {
    /* For simplicity the public API uses union values; consumer manages allocations. */
    uint32_t int_val;
    char *str_val; /* NULL for missing string */
} SbfValue;

typedef struct {
    /* values is an array of length schema->field_count */
    SbfValue *values;
} SbfRecord;

/* Opaque handle for streaming parser (optional) */
typedef struct SbfReader SbfReader;

/* Public API */

/* Save an array of records to path using schema.
 * - path: file path to write
 * - schema: pointer to SbfSchema (must be valid)
 * - records: pointer to first record (array) or NULL if record_count==0
 * - record_count: number of records
 *
 * Returns SBF_OK on success or negative error code.
 */
SBF_API int sbf_save(const char *path, const SbfSchema *schema, const SbfRecord *records, int record_count);

/* Load file; returns 0 on success and allocates:
 * - schema_out : must be non-NULL; filled with parsed schema
 * - records_out: pointer to allocated array of SbfRecord; caller must free with sbf_free_records
 * - record_count_out: number of parsed records
 *
 * On error returns negative code and leaves outputs unspecified.
 */
SBF_API int sbf_load(const char *path, SbfSchema *schema_out, SbfRecord **records_out, int *record_count_out);

/* Free records memory allocated by sbf_load */
SBF_API void sbf_free_records(SbfRecord *records, int record_count, int field_count);

/* Helper: write version of raw magic or inspect header */
SBF_API int sbf_inspect_header(const char *path, uint16_t *version_out, uint16_t *field_count_out);

/* Streaming API (optional, for big files) */
SBF_API SbfReader *sbf_reader_open(const char *path);
SBF_API int sbf_reader_next(SbfReader *r, SbfRecord *out); /* read next record */
SBF_API void sbf_reader_close(SbfReader *r);

#ifdef __cplusplus
}
#endif

#endif /* SBF_H */
