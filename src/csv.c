/*
* NAME: Copyright (c) 2020, Biren Patel
* DESC: CSV file reader implementation
* LISC: MIT License
*/

#include "csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

/*******************************************************************************
Implementation local error codes
*/

enum
{
    SUCCESS = 0,
    UNGETC_FAILED = 1,
    MALLOC_FAILED = 2,
    COLS_OVERFLOW = 3,
    ROWS_OVERFLOW = 4,
};

/*******************************************************************************
Static prototypes
*/

static int csv_get_dims
(
    FILE * const csvfile,
    const bool header,
    uint32_t *rows,
    uint32_t *cols
);

static int csv_get_header(struct csv *csv, FILE * const csvfile);


/*******************************************************************************
File macros
*/

#define CSV_TERMINATE(error_var, error_name, goto_label)                       \
        do                                                                     \
        {                                                                      \
            *error_var = error_name;                                           \
            goto goto_label;                                                   \
        } while (0)                                                            \

/*******************************************************************************
Read a CSV file from disk into memory as an array of struct csv_cell, kept as
the flexible array member of struct csv. First error checking, then fetch the
required array dimensions, then perform the read.
*/

struct csv *csv_read(const char * const filename, const bool header, int *error)
{
    int status = 0;
    
    if (filename == NULL) CSV_TERMINATE(error, CSV_NULL_FILENAME, early_stop);
    if (error == NULL) CSV_TERMINATE(error, CSV_NULL_ERROR_HANDLE, early_stop);
    
    FILE *csvfile = fopen(filename, "rt");
    
    //verify that the file exists and is non-empty
    if (csvfile == NULL) CSV_TERMINATE(error, CSV_INVALID_FILE, early_stop);
    else
    {
        int c = getc(csvfile);
        
        if (c == EOF) CSV_TERMINATE(error, CSV_EMPTY_FILE, fail);
        else rewind(csvfile);
    }
    
    //fetch array dimensions
    uint32_t rows = 0;
    uint32_t cols = 0;
    
    status = csv_get_dims(csvfile, header, &rows, &cols);
    
    if (status == SUCCESS) rewind(csvfile);
    else
    {
        if (status == UNGETC_FAILED)
            CSV_TERMINATE(error, CSV_FATAL_UNGETC_FAILED, fail);
        if (status == COLS_OVERFLOW)
            CSV_TERMINATE(error, CSV_NUM_COLUMNS_OVERFLOW, fail);
        if (status == ROWS_OVERFLOW)
            CSV_TERMINATE(error, CSV_NUM_ROWS_OVERFLOW, fail);
        
        goto fail;
    }
    
    //setup struct csv
    uint64_t total_cells = (uint64_t) rows * (uint64_t) cols;
    uint64_t cell_size = sizeof(struct csv_cell) * total_cells;
    
    struct csv *csv = malloc(sizeof(struct csv) + cell_size);
    
    if (csv == NULL) CSV_TERMINATE(error, CSV_MALLOC_FAILED, fail);
    else
    {
        csv->rows = rows;
        csv->cols = cols;
        csv->total = total_cells;
        
        //no rewind after this so that all paths start at the first data row
        if (header == true) 
        {
            status = csv_get_header(csv, csvfile);
            if (status == MALLOC_FAILED) 
            {
                CSV_TERMINATE(error, CSV_MALLOC_FAILED, fail);
            }
        }
        else csv->header = NULL;
    }
    
    //read each datum into a struct csv_cell
    
    //check all good
    goto success;
    
    //error handling   
    success:
        fclose(csvfile);
        *error = CSV_PARSE_SUCCESSFUL;
        return csv;
        
    fail:
        fclose(csvfile);
        return NULL;
        
    early_stop:
        return NULL;
}


/*******************************************************************************
Determine the dimensions of the csv file for malloc. The number of rows returned
is excluding the header. Full RFC 4180 compliance means that we can bypass a few
error handling routines. It also implies that the first row alone is enough to
determine the number of columns.
*/

static int csv_get_dims
(
    FILE * const csvfile,
    const bool header,
    uint32_t *rows,
    uint32_t *cols
)
{
    int c = 0;
    
    *cols = 1;
    
    //determine number of columns
    while ((c = getc(csvfile)) != EOF)
    {
        switch (c)
        {
            //comma indicates additional column
            case ',':
                (*cols)++;
                if (*cols == 0) return COLS_OVERFLOW;
                break;
            
            //linefeed indicates row termination, check RFC 4180 rule 2 and exit
            case '\n':
                c = getc(csvfile);
                
                if (c == EOF) goto found_eof;
                else
                {
                    if (c != ungetc(c, csvfile)) return UNGETC_FAILED;
                    else goto found_more;
                }
            
            //double quotes -> consume everything until enclosing quote
            //RFC 4180 rule 7 implies quotes incl. escapes always come in pairs
            case '"':
                while (1)
                {
                    c = getc(csvfile);
                    if (c == '"') break;
                    else continue;
                }
                break;
                
            default:
                break;
        }
    }
    
    //by RFC 4180 grammar, header is guaranteed false and there is one data row.
    found_eof:
        assert (*cols > 0 && "no columns found");
        *rows = 1;
        return SUCCESS;

    //the header is not counted in row dimensions
    found_more:
        assert (*cols > 0 && "no columns found");
        if (header == true) *rows = 0;
        else *rows = 1;
        
    //determine the number of rows
    while ((c = getc(csvfile)) != EOF)
    {
        switch (c)
        {
            case '\n':
                c = getc(csvfile);
                
                if (c == EOF) goto terminate;
                else
                {
                    if (c != ungetc(c, csvfile)) return UNGETC_FAILED;
                    (*rows)++;
                    if (*rows == 0) return ROWS_OVERFLOW;
                }
                
                break;
                
            case '"':
                while (1)
                {
                    c = getc(csvfile);
                    if (c == '"') break;
                    else continue;
                }
                break;
                
            default:
                break;
        }
    }
    
    //RFC 4180 rule 2 exception to account for last row ending without linefeed
    terminate:
        (*rows)++;
        if (*rows == 0) return ROWS_OVERFLOW;
    
    assert (*rows >= 1 && "one or zero columns found, goto error");
    
    return SUCCESS;
}


/*******************************************************************************
If the CSV file contains a header, dynamically allocate a ragged array of column
names and assign to struct csv metadata. By RFC 4180 rule 3, the header has the
same format as the records.
*/

static int csv_get_header(struct csv *csv, FILE * const csvfile)
{
    uint32_t i = 0;
    int c = 0;
    uint64_t lag = 0;
    uint64_t lead = 0;
    char *tmp = NULL;
    
    csv->header = malloc(sizeof(void*) * (uint64_t) csv->rows);
    if (csv->header == NULL) return MALLOC_FAILED;
    
    while (i != csv->cols)
    {
        c = getc(csvfile);
        
        switch (c)
        {
            //quoted fields need to backtrack the lead pointer to ignore quotes
            case '"':
                lag++;
                lead++;
                break;
            
            //unquoted fields read straight through until comma or linefeed
            default:
                while (!(c == ',' || c == '\n')) 
                {
                    lead++;
                    c = getc(csvfile);
                }
                
                tmp = malloc(sizeof(char) * (lead - lag) + 1);
                if (tmp == NULL) return MALLOC_FAILED;
                
                //copy column name directly from file to header array
                fsetpos(csvfile, (const fpos_t *) &lag);
                fread(tmp, sizeof(char), lead - lag, csvfile);
                tmp[lead - lag] = '\0';
                csv->header[i] = tmp;
                
                //reset lag, lead, and file pos at next column
                ++lead;
                fsetpos(csvfile, (const fpos_t *) &lead);
                lag = lead;
                
                break;
        }
        
        i++;
    }
    
    return SUCCESS;
}


/******************************************************************************/

#undef CSV_TERMINATE
