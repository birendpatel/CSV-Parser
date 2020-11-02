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
* @ error : enumerated error code
*******************************************************************************/
struct csv *csv_read(const char * const filename, const bool header, int *error);

enum
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
    CSV_UNKNOWN_FATAL_ERROR     = 9,
};

/*******************************************************************************
* NAME: csv_free
* DESC: destroy struct csv and free all dynamically allocated memory
* OUTP: none
*******************************************************************************/
void csv_free(struct csv *csv);

#endif
