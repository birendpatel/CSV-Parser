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
* DESC: implementation uses by default a 1 KiB buffer to process each csv cell. 
* NOTE: temporary buffers are allocated on the heap
* NOTE: can be modified to any positive value depending on the expected types
*******************************************************************************/
#define CSV_TEMPORARY_BUFFER_LENGTH 1024

/*******************************************************************************
* NAME: csv_error_code
* DESC: all possible error codes that may be returned by any function
*******************************************************************************/
enum csv_error_code
{
    CSV_PARSE_SUCCESSFUL        = 0,
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
};

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
struct csv *csv_read(const char * const filename, const bool header, int *error);

/*******************************************************************************
* NAME: csv_free
* DESC: destroy struct csv and free all dynamically allocated memory
* OUTP: none
*******************************************************************************/
void csv_free(struct csv *csv);

/*******************************************************************************
* NAME: csv_row_as_*
* DESC: return row i as a dynamically allocated array of the wildcard type
* OUTP: null if failure to transform any cell to the requested type
* NOTE: user responsibility to free returned array
* @ error : contains error code on return if not null
*******************************************************************************/
long *csv_row_as_long(struct csv *csv, const uint32_t i, int *error);

#endif
