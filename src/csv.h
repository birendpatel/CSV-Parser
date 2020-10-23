/*
* NAME: Copyright (c) 2020, Biren Patel
* DESC: Read a RFC 4180 compliant CSV file into memory with inferred types
* LISC: MIT License
*/

#ifndef CSV_H
#define CSV_H

#include <stdint.h>

/*******************************************************************************
* NAME: struct csv_cell
* DESC: in-memory representation of a single value of the csv file
* @ type : union tag
* @ status : missing if data not found during parsing
* @ value : string type data is dynamically allocated and null terminated
*******************************************************************************/
struct csv_cell
{
    enum {INTEGER_TYPE, FLOATING_TYPE, STRING_TYPE, CHAR_TYPE} type;
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
* @ data : dimensions rows x cols
*******************************************************************************/
struct csv
{
    uint32_t rows;
    uint32_t cols;
    uint32_t missing;
    uint32_t total;
    struct csv_cell data[];
} csv;

/*******************************************************************************
* NAME: csv_read
* DESC: read a RFC 4180 compliant csv file into memory
* OUTP: error code macro
* @ csv : on success a pointer to dynamically allocated struct csv
* @ fname : csv filename
* @ sep : override RFC 4180 with non-compliant separator
*******************************************************************************/
int csv_read(struct csv *csv, const char *fname, const char sep);

#define CSV_NULL_INPUT_POINTER
#define CSV_INVALID_FILE_NAME
#define CSV_MALLOC_FAILED

/*******************************************************************************
* NAME: csv_free
* DESC: destroy struct csv and free all dynamically allocated memory
* OUTP: none
*******************************************************************************/
void csv_free(struct csv *csv);

#endif