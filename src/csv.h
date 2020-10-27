/*
* NAME: Copyright (c) 2020, Biren Patel
* DESC: Read a RFC 4180 compliant CSV file into memory with inferred types
* LISC: MIT License
*/

#ifndef CSV_H
#define CSV_H

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
* NAME: struct csv_cell
* DESC: in-memory representation of a single value of the csv file
* @ type : union tag
* @ status : missing if data not found during parsing
* @ value : string type data is dynamically allocated and null terminated
*******************************************************************************/
struct csv_cell
{
    enum {INTEGER_TYPE, FLOATING_TYPE, STRING_TYPE, CHAR_TYPE, NONE_TYPE} type;
    enum {MISSING, PRESENT} status;
    union
    {
        int64_t intval;
        double  dblval;
        char*   strval;
        char    chrval;
    } value;
};

/*******************************************************************************
* NAME: struct csv
* DESC: in-memory representation of the entire csv file
* @ rows : total rows
* @ cols : total columns
* @ missing : total missing values
* @ total : total values parsed, including missing values
* @ header: array of column names, null when header not available
* @ data : dimensions rows x cols
*******************************************************************************/
struct csv
{
    uint32_t rows;
    uint32_t cols;
    uint64_t missing;
    uint64_t total;
    char **header;
    struct csv_cell data[];
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
    CSV_NULL_FILENAME           = 100,
    CSV_INVALID_FILE            = 101,
    CSV_NUM_COLUMNS_OVERFLOW    = 200,
    CSV_NUM_ROWS_OVERFLOW       = 201,
    CSV_MALLOC_FAILED           = 300,
    CSV_FATAL_UNGETC            = 400,
};

/*******************************************************************************
* NAME: csv_free
* DESC: destroy struct csv and free all dynamically allocated memory
* OUTP: none
*******************************************************************************/
void csv_free(struct csv *csv);

#endif
