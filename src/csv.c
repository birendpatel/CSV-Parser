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
#include <string.h>

/*******************************************************************************
Static prototypes
*/

static int csv_dims(FILE * const csvfile, bool header, uint32_t *r, uint32_t *c);
static int csv_cols(FILE * const csvfile, uint32_t *cols);
static int csv_rows(FILE * const csvfile, uint32_t *rows);
static int tokenize(FILE * const csvfile, char *buffer, int n);
static int csv_get_header(struct csv *csv, FILE * const csvfile, fpos_t *pos);
static int csv_get_data(struct csv *csv, FILE * const csvfile, fpos_t data_pos);

/*******************************************************************************
File macros
*/

#define STOP(error_var, error_name, goto_label)                                \
        do                                                                     \
        {                                                                      \
            *error_var = error_name;                                           \
            goto goto_label;                                                   \
        } while (0)                                                            \
        
#define CSV_SIZE sizeof(struct csv)
#define CELL_SIZE sizeof(struct csv_cell)
#define SUCCESS 0
#define UNDEFINED 999

/*******************************************************************************
Read a CSV file from disk into memory as a 2D array of csv cells. 
*/

struct csv *csv_read(const char * const filename, const bool header, int *error)
{
    int status = UNDEFINED;
    uint32_t rows = 0;
    uint32_t cols = 0;
    uint64_t total = 0;
    fpos_t data_pos = 0;
    
    if (error == NULL) goto early_stop;
    
    //verify and open file
    if (filename == NULL) STOP(error, CSV_NULL_FILENAME, early_stop);
    FILE *csvfile = fopen(filename, "rt");
    if (csvfile == NULL) STOP(error, CSV_INVALID_FILE, early_stop);
    
    //fetch array dimensions
    status = csv_dims(csvfile, header, &rows, &cols);
    if (status != SUCCESS) STOP(error, status, fail);
    
    //configure struct csv
    total = (uint64_t) rows * cols;
    struct csv *csv = malloc(CSV_SIZE + CELL_SIZE * total);
    if (csv == NULL) STOP(error, CSV_MALLOC_FAILED, fail);
    
    csv->rows = rows;
    csv->cols = cols;
    csv->total = total;
    
    //fetch header    
    if (header == false) csv->header = NULL;
    else
    {
        csv->header = malloc(sizeof(void*) * (uint64_t) csv->rows);
        if (csv->header == NULL) STOP(error, CSV_MALLOC_FAILED, fail);
        
        status = csv_get_header(csv, csvfile, &data_pos);
        if (status != SUCCESS) STOP(error, status, fail);
    }
    
    //read each datum into a struct csv_cell       
    csv_get_data(csv, csvfile, data_pos);
    
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
error handling routines.
*/

static int csv_dims(FILE * const csvfile, bool header, uint32_t *r, uint32_t *c)
{
    int status = UNDEFINED;
    
    status = csv_cols(csvfile, c);
    
    if (status != SUCCESS) return status;
    
    status = csv_rows(csvfile, r);
    
    if (status == SUCCESS)
    {
        if (header == true) (*r)--;
        rewind(csvfile);
    }
    
    return status;
}

/*******************************************************************************
Read the first row of the CSV file to calculate total columns. RFC implies that 
the first row alone is enough to determine the number of columns. For double
quotes, RFC 4180 rule 7 implies all quotes always come in pairs, so we consume
two quotes at a time without risk, even when they are not the correct pairing.
*/

static int csv_cols(FILE * const csvfile, uint32_t *cols)
{
    int c = 0;
    *cols = 1;
    
    rewind(csvfile);
    
    while ((c = getc(csvfile)) != EOF)
    {
        switch (c)
        {
            case ',':
                (*cols)++;
                if (*cols == 0) return CSV_NUM_COLUMNS_OVERFLOW;
                break;
            
            case '\n':
                return SUCCESS;
            
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
    
    return SUCCESS;
}


/*******************************************************************************
Calculate total rows, including both the header and the actual data rows. Avoid
double responsibility so the manager will handle the logic to exclude the header 
from the total count.
*/

static int csv_rows(FILE * const csvfile, uint32_t *rows)
{
    int c = 0;
    *rows = 0;
    
    rewind(csvfile);
    
    while ((c = getc(csvfile)) != EOF)
    {
        switch (c)
        {
            //RFC rule 2 branch for EOF + CRLF
            case '\n':
                c = getc(csvfile);
                
                if (c == EOF) goto terminate;
                else
                {
                    if (c != ungetc(c, csvfile)) return CSV_FATAL_UNGETC;
                    (*rows)++;
                    if (*rows == 0) return CSV_NUM_ROWS_OVERFLOW;
                }
                
                break;
            
            //same as csv_cols, consume until closing quote
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
    
    //RFC 4180 rule 2 exception to account for no CRLF on final row
    terminate:
        (*rows)++;
        if (*rows == 0) return CSV_NUM_ROWS_OVERFLOW;
    
    return SUCCESS;
}


/*******************************************************************************
Field tokenizer. Read next field from current file position and write it into
the target buffer as a nul-terminated string. EOF condition is not used in the
header processing. Enclosing quotes and escape sequence quotes are removed.
*/

static int tokenize(FILE * const csvfile, char *buffer, int n)
{
    int lag = 0;
    int lead = 0;
    uint32_t i = 0;
    
    while (1)
    {
        lag = getc(csvfile);
        
        if (lag == '"')
        {
            lead = getc(csvfile);
            
            if (lead == ',' || lead == '\n' || lead == EOF) break;
            else buffer[i++] = (char) lead;
        }
        else if (lag == ',' || lag == '\n' || lag == EOF) break;
        else buffer[i++] = (char) lag;
        
        if (i == 0) return CSV_FIELD_LEN_OVERFLOW;
    }
    
    if (i == (uint32_t) n) return CSV_BUFFER_OVERFLOW;
    
    buffer[i] = '\0';
    
    return SUCCESS;
}


/*******************************************************************************
If the CSV file contains a header, dynamically allocate a ragged array of column
names and assign to struct csv metadata. By RFC 4180 rule 3, the header has the
same format as the records. Note the file position as the start of the data.
*/

static int csv_get_header(struct csv *csv, FILE * const csvfile, fpos_t *pos)
{
    int status = UNDEFINED;
    char *tmp_field = NULL;
    
    char *tmp = malloc(CSV_TEMPORARY_BUFFER_LENGTH);
    if (tmp == NULL) return CSV_MALLOC_FAILED;
    
    rewind(csvfile);
    
    for (uint32_t i = 0; i < csv->cols; i++)
    {
        //load temp buffer with next field
        status = tokenize(csvfile, tmp, CSV_TEMPORARY_BUFFER_LENGTH);
        if (status != SUCCESS) return status;
        
        //copy temp to size-appropriate block and save
        tmp_field = malloc(strlen(tmp) + 1);
        if (tmp_field == NULL) return CSV_MALLOC_FAILED;
        strncpy(tmp_field, tmp, strlen(tmp) + 1);
        csv->header[i] = tmp_field;
    }
    
    free(tmp);
    fgetpos(csvfile, pos);
    
    return SUCCESS;    
}


/*******************************************************************************
Load flexible array of struct csv with all of the file contents. Read a field
one by one into a temporary buffer. Analyze the field to infer its type. Then
perform some extra analysis if required to set columns to homogenous types using
sampling and frequency lists.
*/

static int csv_get_data(struct csv *csv, FILE * const csvfile, fpos_t data_pos)
{
    int status = UNDEFINED;
    uint64_t missing = 0;
    
    char *tmp = malloc(CSV_TEMPORARY_BUFFER_LENGTH);
    if (tmp == NULL) return CSV_MALLOC_FAILED;
    
    fsetpos(csvfile, &data_pos);
    
    for (uint32_t i = 0; i < csv->rows; i++)
    {
        for (uint32_t j = 0; j < csv->cols; j++)
        {
            //load temp buffer with next field
            status = tokenize(csvfile, tmp, CSV_TEMPORARY_BUFFER_LENGTH);
            if (status != SUCCESS) return status;
            
            puts(tmp);
        }
    }
    
    free(tmp);
    return SUCCESS;
}
