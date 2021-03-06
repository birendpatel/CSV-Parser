/*
* NAME: Copyright (c) 2020, Biren Patel
* DESC: Read a RFC 4180 compliant CSV file into memory as a 2D array of strings
* LISC: MIT License
*/

#ifndef CSV_H
#define CSV_H

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
* NAME: CSV_TEMPORARY_BUFFER_LENGTH
* DESC: implementation uses default 1 KiB heap buffer to process each field
* NOTE: overflow during csv processing wil result in CSV_BUFFER_OVERFLOW error
*******************************************************************************/
#define CSV_TEMPORARY_BUFFER_LENGTH 1024

/*******************************************************************************
* NAME: csv_error_t
* DESC: API error codes
* NOTE: use CSV_UNDEFINED to silence -Wuninitialized
* NOTE: pass error code to csv_errno_decode() for error details
*******************************************************************************/
typedef enum
{
    CSV_SUCCESS                 = 0,
    CSV_NULL_FILENAME           = 1,
    CSV_INVALID_FILE            = 2,
    CSV_NUM_COLUMNS_OVERFLOW    = 3,
    CSV_NUM_ROWS_OVERFLOW       = 4,
    CSV_FIELD_LEN_OVERFLOW      = 5,
    CSV_BUFFER_OVERFLOW         = 6,
    CSV_MALLOC_FAILED           = 7,
    CSV_FATAL_UNGETC            = 8,
    CSV_NULL_INPUT_POINTER      = 9,
    CSV_PARAM_OUT_OF_BOUNDS     = 10,
    CSV_UNKNOWN_FATAL_ERROR     = 11,
    CSV_READ_FAIL               = 12,
    CSV_READ_OVERFLOW           = 13,
    CSV_READ_UNDERFLOW          = 14,
    CSV_READ_PARTIAL            = 15,
    CSV_INVALID_BASE            = 16,
    CSV_MISSING_DATA            = 17,
    CSV_UNDEFINED               = 999
} csv_errno;

const char *csv_errno_decode(const csv_errno error);

/*******************************************************************************
* NAME: struct csv
* DESC: in-memory representation of the entire csv file
* @ rows : total rows
* @ cols : total columns
* @ missing : total missing values
* @ total : total values parsed, including missing values
* @ header: array of column names, null when header not available
* @ data : rows X cols 3D ragged array. Element is null pointer when missing.
*******************************************************************************/
struct csv
{
    uint32_t rows;
    uint32_t cols;
    uint64_t missing;
    uint64_t total;
    char **header;
    char ***data;
};

/*******************************************************************************
* NAME: csv_read
* DESC: read a RFC 4180 compliant csv file into memory
* OUTP: dynamically allocated struct csv, if null check error arg for details
* @ filename : csv filename
* @ header : true if first row of csv file contains column headers
* @ error : contains error code on return if not null
*******************************************************************************/
struct csv *csv_read(const char * const filename, const bool header, csv_errno *error);

/*******************************************************************************
* NAME: csv_free
* DESC: destroy struct csv and free all dynamically allocated memory
* OUTP: none
*******************************************************************************/
void csv_free(struct csv *csv);

/*******************************************************************************
* NAME: csv_row[*]
* DESC: return row i as a dynamically allocated array of the wildcard type
* OUTP: null if failure to transform any cell to the requested type
* NOTE: user responsibility to free returned array
* @ error : contains error code on return if not null
*******************************************************************************/
long *csv_rowl(struct csv *csv, const uint32_t i, const int base, csv_errno *error);
char *csv_rowc(struct csv *csv, const uint32_t i, csv_errno *error);
double *csv_rowd(struct csv *csv, const uint32_t i, csv_errno *error);

/*******************************************************************************
* NAME: csv_col[*]
* DESC: return col j as a dynamically allocated array of the wildcard type
* OUTP: null if failure to transform any cell to the requested type
* NOTE: user responsibility to free returned array
* @ error : contains error code on return if not null
*******************************************************************************/
long *csv_coll(struct csv *csv, const uint32_t j, const int base, csv_errno *error);
char *csv_colc(struct csv *csv, const uint32_t j, csv_errno *error);
double *csv_cold(struct csv *csv, const uint32_t j, csv_errno *error);

#endif
