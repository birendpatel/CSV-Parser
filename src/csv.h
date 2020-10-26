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
    char *header;
    struct csv_cell data[];
};

/*******************************************************************************
* NAME: csv_init
* DESC: syntactic sugar to suppress -Wuninitialized
*******************************************************************************/
#define csv_init() NULL

/*******************************************************************************
* NAME: csv_read
* DESC: read a RFC 4180 compliant csv file into memory
* OUTP: enumerated error code
* @ filename : csv filename
* @ header : true if first row of csv file contains column headers
*******************************************************************************/
int csv_read(struct csv *csv, const char * const filename, const bool header);

enum
{
    CSV_PARSE_SUCCESSFUL        = 0,
    CSV_NULL_FILENAME           = 1,
    CSV_INVALID_FILE            = 2,
    CSV_MALLOC_FAILED           = 3,
    CSV_EMPTY_FILE              = 4,
    CSV_FATAL_UNGETC_FAILED     = 5,
};

/*******************************************************************************
* NAME: csv_free
* DESC: destroy struct csv and free all dynamically allocated memory
* OUTP: none
*******************************************************************************/
void csv_free(struct csv *csv);

#endif
