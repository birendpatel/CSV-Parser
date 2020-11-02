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
#include <errno.h>

/*******************************************************************************
Static prototypes
*/

static int csv_dims(FILE * const csvfile, bool header, uint32_t *r, uint32_t *c);
static int csv_cols(FILE * const csvfile, uint32_t *cols);
static int csv_rows(FILE * const csvfile, uint32_t *rows);
static int csv_tokenize(FILE * const csvfile, char *buffer, int n);
static int csv_get_header(struct csv *csv, FILE * const csvfile, fpos_t *pos);
static int csv_get_data(struct csv *csv, FILE * const csvfile, fpos_t data_pos);

/*******************************************************************************
File macros
*/

#define STOP(error_var, error_name, goto_label)                                \
        do                                                                     \
        {                                                                      \
            if (error != NULL) *error_var = error_name;                        \
            goto goto_label;                                                   \
        } while (0)                                                            \

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
    fpos_t data_pos = 0;
    
    //verify and open file
    if (filename == NULL) STOP(error, CSV_NULL_FILENAME, early_stop);
    FILE *csvfile = fopen(filename, "rt");
    if (csvfile == NULL) STOP(error, CSV_INVALID_FILE, early_stop);
    
    //fetch array dimensions
    status = csv_dims(csvfile, header, &rows, &cols);
    if (status != SUCCESS) STOP(error, status, fail);
    
    //configure struct csv
    struct csv *csv = malloc(sizeof(struct csv));
    if (csv == NULL) STOP(error, CSV_MALLOC_FAILED, fail);
    
    csv->rows = rows;
    csv->cols = cols;
    csv->total = (uint64_t) rows * cols;
    
    //fetch header    
    if (header == false) csv->header = NULL;
    else
    {       
        status = csv_get_header(csv, csvfile, &data_pos);
        if (status != SUCCESS) STOP(error, status, fail);
    }
    
    //read each datum into a 2D array of strings (3D ragged array)    
    status = csv_get_data(csv, csvfile, data_pos);
    if (status != SUCCESS) STOP(error, status, fail);
    
    //sanity checks
    if (csv->total <= csv->missing) STOP(error, CSV_UNKNOWN_FATAL_ERROR, fail);
    else goto success;
    
    //error handling
    success:
        fclose(csvfile);
        if (error != NULL) *error = CSV_PARSE_SUCCESSFUL;
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

static int csv_tokenize(FILE * const csvfile, char *buffer, int n)
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
    
    rewind(csvfile);
    
    //intermediate buffer for each file read
    char *tmp = malloc(CSV_TEMPORARY_BUFFER_LENGTH);
    if (tmp == NULL) return CSV_MALLOC_FAILED;
    
    //final target for header contents
    csv->header = malloc(sizeof(void*) * (uint64_t) csv->rows);
    if (csv->header == NULL) return CSV_MALLOC_FAILED;
    
    for (uint32_t i = 0; i < csv->cols; i++)
    {
        //load temp buffer with next field
        status = csv_tokenize(csvfile, tmp, CSV_TEMPORARY_BUFFER_LENGTH);
        if (status != SUCCESS) return status;
        
        //copy temp to size-appropriate block and save
        csv->header[i] = malloc(strlen(tmp) + 1);
        if (csv->header[i] == NULL) return CSV_MALLOC_FAILED;
        strncpy(csv->header[i], tmp, strlen(tmp) + 1);
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
    csv->missing = 0;
    
    fsetpos(csvfile, &data_pos);
    
    //intermediate buffer for each file read
    char *tmp = malloc(CSV_TEMPORARY_BUFFER_LENGTH);
    if (tmp == NULL) return CSV_MALLOC_FAILED;
    
    //alloc data as a 2D array for [i][j] indexing
    csv->data = malloc(sizeof(void*) * (uint64_t) csv->rows);
    if (csv->data == NULL) return CSV_MALLOC_FAILED;
    
    for (uint32_t i = 0; i < csv->rows; i++)
    {
        csv->data[i] = malloc(sizeof(void*) * (uint64_t) csv->cols);
        if (csv->data[i] == NULL) return CSV_MALLOC_FAILED;   
    }
    
    for (uint32_t i = 0; i < csv->rows; i++)
    {
        for (uint32_t j = 0; j < csv->cols; j++)
        {
            //load temp buffer with next field
            status = csv_tokenize(csvfile, tmp, CSV_TEMPORARY_BUFFER_LENGTH);
            if (status != SUCCESS) return status;
            
            //load csv cell
            if (tmp[0] == '\0')
            {
                csv->data[i][j] = NULL;
                csv->missing++;
            }
            else
            {
                csv->data[i][j] = malloc(strlen(tmp) + 1);
                if (csv->data[i][j] == NULL) return CSV_MALLOC_FAILED;
                strncpy(csv->data[i][j], tmp, strlen(tmp) + 1);
            }
        }
    }
    
    free(tmp);
    return status;
}


/*******************************************************************************
Quite a lot of dynamic allocations happened during csv_read. First release the 
headers, then release char data pointers, release column arrays, release row
arrays, and finally release the struct itself. DrMemory double checks everything
in the unit test source.
*/

void csv_free(struct csv *csv)
{    
    for (uint32_t i = 0; i < csv->cols; i++)
    {
        free(csv->header[i]);
    }
    
    free(csv->header);
    
    for (uint32_t i = 0; i < csv->rows; i++)
    {        
        for (uint32_t j = 0; j < csv->cols; j++)
        {
            free(csv->data[i][j]);
        }
        
        free(csv->data[i]);
    }
    
    free(csv->data);
    
    free(csv);
}


/*******************************************************************************

*/

long *csv_row_as_long(struct csv *csv, const uint32_t i, int *error)
{
    if (csv == NULL) STOP(error, CSV_NULL_INPUT_POINTER, early_stop);
    if (i >= csv->rows) STOP(error, CSV_PARAM_OUT_OF_BOUNDS, early_stop);
    
    long *data = malloc(sizeof(int) * csv->cols);
    if (data == NULL) STOP(error, CSV_MALLOC_FAILED, early_stop);

    for (uint32_t j = 0; j < csv->cols; j++)
    {
        errno = 0;
        long tmp = 0;
        char *end = NULL;
        
        tmp = strtol(csv->data[i][j], &end, 10);
        
        //parse error handling
        if (csv->data[i][j] == end) 
            STOP(error, CSV_READ_FAIL, fail);
        if (errno == ERANGE && tmp == LONG_MIN)
            STOP(error, CSV_READ_UNDERFLOW, fail);
        if (errno == ERANGE && tmp == LONG_MAX)
            STOP(error, CSV_READ_OVERFLOW, fail);
        if (errno == 0 && *end != '\0')
            STOP(error, CSV_READ_PARTIAL, fail);
        if (errno != 0)
            STOP(error, CSV_UNKNOWN_FATAL_ERROR, fail);
        
        //success
        data[j] = tmp;
    }
    
    if (error != NULL) *error = CSV_PARSE_SUCCESSFUL;
    return data;
    
    fail:
        free(data);
        return NULL;
    
    early_stop:
        return NULL;
}
