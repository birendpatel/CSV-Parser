/*
* NAME: Copyright (c) 2020, Biren Patel
* DESC: CSV file reader implementation
* LISC: MIT License
*/

#include "csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


/*******************************************************************************
Implementation local error codes
*/

enum
{
    SUCCESS = 0,
    UNGETC_FAILED = 1
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
Read a CSV file from disk into memory as an array of struct csv_cell, kept as
the flexible array member of struct csv. First error checking, then fetch the
required array dimensions, then perform the read.
*/

int csv_read(struct csv *csv, const char * const filename, const bool header)
{
    int status = 0;
    
    if (filename == NULL) return CSV_NULL_FILENAME;
    
    FILE *csvfile = fopen(filename, "rt");
    
    //verify that the file exists and is non-empty
    if (csvfile == NULL) return CSV_INVALID_FILE;
    else
    {
        int c = getc(csvfile);
        
        if (c == EOF)
        {
            fclose(csvfile);
            return CSV_EMPTY_FILE;
        }    
        else rewind(csvfile);
    }
    
    //fetch array dimensions
    uint32_t rows = 0;
    uint32_t cols = 0;
    
    status = csv_get_dims(csvfile, header, &rows, &cols);
    
    if (status == UNGETC_FAILED) 
    {
        fclose(csvfile);
        return CSV_FATAL_UNGETC_FAILED;
    }
    else rewind(csvfile);
    
    //setup struct csv
    uint64_t total_cells = (uint64_t) rows * (uint64_t) cols;
    
    csv = malloc(sizeof(struct csv) + sizeof(struct csv_cell) * total_cells);
    
    if (csv == NULL)
    {
        fclose(csvfile);
        return CSV_MALLOC_FAILED;
    }
    else
    {
        csv->rows = rows;
        csv->cols = cols;
        csv->total = total_cells;
        
        //no rewind if true to make data read easier
        if (header == true) csv_get_header(csv, csvfile);
        else csv->header = NULL;
    }
    
    //read each datum into a struct csv_cell
    
    return CSV_PARSE_SUCCESSFUL;
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
    //
}
