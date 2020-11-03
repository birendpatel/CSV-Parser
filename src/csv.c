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

static csv_errno csv_dims(FILE * const csvfile, bool header, uint32_t *r, uint32_t *c);
static csv_errno csv_cols(FILE * const csvfile, uint32_t *cols);
static csv_errno csv_rows(FILE * const csvfile, uint32_t *rows);
static csv_errno csv_tokenize(FILE * const csvfile, char *buffer, int n);
static csv_errno csv_get_header(struct csv *csv, FILE * const csvfile, fpos_t *pos);
static csv_errno csv_get_data(struct csv *csv, FILE * const csvfile, fpos_t data_pos);

/*******************************************************************************
File macros
*/

#define STOP(error_var, error_name, goto_label)                                \
        do                                                                     \
        {                                                                      \
            if (error != NULL) *error_var = error_name;                        \
            goto goto_label;                                                   \
        } while (0)                                                            \

/*******************************************************************************
Read a CSV file from disk into memory as a 2D array of csv cells. 
*/

struct csv *csv_read(const char * const filename, const bool header, csv_errno *error)
{
    csv_errno status = CSV_UNDEFINED;
    uint32_t rows = 0;
    uint32_t cols = 0;
    fpos_t data_pos = 0;
    
    //verify and open file
    if (filename == NULL) STOP(error, CSV_NULL_FILENAME, early_stop);
    FILE *csvfile = fopen(filename, "rt");
    if (csvfile == NULL) STOP(error, CSV_INVALID_FILE, early_stop);
    
    //fetch array dimensions
    status = csv_dims(csvfile, header, &rows, &cols);
    if (status != CSV_SUCCESS) STOP(error, status, fail);
    
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
        if (status != CSV_SUCCESS) STOP(error, status, fail);
    }
    
    //read each datum into a 2D array of strings (3D ragged array)    
    status = csv_get_data(csv, csvfile, data_pos);
    if (status != CSV_SUCCESS) STOP(error, status, fail);
    
    //sanity checks
    if (csv->total <= csv->missing) STOP(error, CSV_UNKNOWN_FATAL_ERROR, fail);
    else goto success;
    
    //error handling
    success:
        fclose(csvfile);
        if (error != NULL) *error = CSV_SUCCESS;
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

static csv_errno csv_dims(FILE * const csvfile, bool header, uint32_t *r, uint32_t *c)
{
    csv_errno status = CSV_UNDEFINED;
    
    status = csv_cols(csvfile, c);
    
    if (status != CSV_SUCCESS) return status;
    
    status = csv_rows(csvfile, r);
    
    if (status == CSV_SUCCESS)
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

static csv_errno csv_cols(FILE * const csvfile, uint32_t *cols)
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
                return CSV_SUCCESS;
            
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
    
    return CSV_SUCCESS;
}


/*******************************************************************************
Calculate total rows, including both the header and the actual data rows. Avoid
double responsibility so the manager will handle the logic to exclude the header 
from the total count.
*/

static csv_errno csv_rows(FILE * const csvfile, uint32_t *rows)
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
    
    return CSV_SUCCESS;
}


/*******************************************************************************
Field tokenizer. Read next field from current file position and write it into
the target buffer as a nul-terminated string. EOF condition is not used in the
header processing. Enclosing quotes and escape sequence quotes are removed.
*/

static csv_errno csv_tokenize(FILE * const csvfile, char *buffer, int n)
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
    
    return CSV_SUCCESS;
}


/*******************************************************************************
If the CSV file contains a header, dynamically allocate a ragged array of column
names and assign to struct csv metadata. By RFC 4180 rule 3, the header has the
same format as the records. Note the file position as the start of the data.
*/

static csv_errno csv_get_header(struct csv *csv, FILE * const csvfile, fpos_t *pos)
{
    csv_errno status = CSV_UNDEFINED;
    
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
        if (status != CSV_SUCCESS) return status;
        
        //copy temp to size-appropriate block and save
        csv->header[i] = malloc(strlen(tmp) + 1);
        if (csv->header[i] == NULL) return CSV_MALLOC_FAILED;
        strncpy(csv->header[i], tmp, strlen(tmp) + 1);
    }
    
    free(tmp);
    fgetpos(csvfile, pos);
    
    return CSV_SUCCESS;    
}


/*******************************************************************************
Load flexible array of struct csv with all of the file contents. Read a field
one by one into a temporary buffer and copy the minimal amount of memory space
to data. Missing fields are allocated as the nul character, rather than being
set as a null pointer. In practice this has made working with the data easier.
*/

static csv_errno csv_get_data(struct csv *csv, FILE * const csvfile, fpos_t data_pos)
{
    csv_errno status = CSV_UNDEFINED;
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
            if (status != CSV_SUCCESS) return status;
            
            //load csv cell
            if (tmp[0] == '\0') csv->missing++;
            
            csv->data[i][j] = malloc(strlen(tmp) + 1);
            if (csv->data[i][j] == NULL) return CSV_MALLOC_FAILED;
            strncpy(csv->data[i][j], tmp, strlen(tmp) + 1);
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
Attempt to convert a row of data to an array of longs. Assumes that data is not
missing.
*/

long *csv_rowl(struct csv *csv, const uint32_t i, const int base, csv_errno *error)
{
    if (csv == NULL) STOP(error, CSV_NULL_INPUT_POINTER, early_stop);
    if (i >= csv->rows) STOP(error, CSV_PARAM_OUT_OF_BOUNDS, early_stop);
    
    long *data = malloc(sizeof(long) * csv->cols);
    if (data == NULL) STOP(error, CSV_MALLOC_FAILED, early_stop);

    for (uint32_t j = 0; j < csv->cols; j++)
    {
        errno = 0;
        long tmp = 0;
        char *end = NULL;
        
        if (csv->data[i][j][0] == '\0') STOP(error, CSV_MISSING_DATA, fail);
        else tmp = strtol(csv->data[i][j], &end, base);
        
        //parse error handling
        if (end == csv->data[i][j]) 
            STOP(error, CSV_READ_FAIL, fail);
        if (errno == ERANGE && tmp == LONG_MIN)
            STOP(error, CSV_READ_UNDERFLOW, fail);
        if (errno == ERANGE && tmp == LONG_MAX)
            STOP(error, CSV_READ_OVERFLOW, fail);
        if (errno == EINVAL)
            STOP(error, CSV_INVALID_BASE, fail);
        if (errno == 0 && *end != '\0')
            STOP(error, CSV_READ_PARTIAL, fail);
        if (errno != 0)
            STOP(error, CSV_UNKNOWN_FATAL_ERROR, fail);
        
        //success
        assert(*end == '\0' && "missing condition");
        data[j] = tmp;
    }
    
    if (error != NULL) *error = CSV_SUCCESS;
    return data;
    
    fail:
        free(data);
        return NULL;
    
    early_stop:
        return NULL;
}

/*******************************************************************************
Attempt to convert a column of data to an array of longs. Assumes that data is
not missing. Will likely be slower than csv_rowl due to cache thrashing.
*/

long *csv_coll(struct csv *csv, const uint32_t j, const int base, csv_errno *error)
{
    if (csv == NULL) STOP(error, CSV_NULL_INPUT_POINTER, early_stop);
    if (j >= csv->cols) STOP(error, CSV_PARAM_OUT_OF_BOUNDS, early_stop);
    
    long *data = malloc(sizeof(long) * csv->rows);
    if (data == NULL) STOP(error, CSV_MALLOC_FAILED, early_stop);

    for (uint32_t i = 0; i < csv->rows; i++)
    {
        errno = 0;
        long tmp = 0;
        char *end = NULL;
        
        if (csv->data[i][j][0] == '\0') STOP(error, CSV_MISSING_DATA, fail);
        else tmp = strtol(csv->data[i][j], &end, base);
        
        //parse error handling
        if (end == csv->data[i][j]) 
            STOP(error, CSV_READ_FAIL, fail);
        if (errno == ERANGE && tmp == LONG_MIN)
            STOP(error, CSV_READ_UNDERFLOW, fail);
        if (errno == ERANGE && tmp == LONG_MAX)
            STOP(error, CSV_READ_OVERFLOW, fail);
        if (errno == EINVAL)
            STOP(error, CSV_INVALID_BASE, fail);
        if (errno == 0 && *end != '\0')
            STOP(error, CSV_READ_PARTIAL, fail);
        if (errno != 0)
            STOP(error, CSV_UNKNOWN_FATAL_ERROR, fail);
        
        //success
        assert(*end == '\0' && "missing condition");
        data[i] = tmp;
    }
    
    if (error != NULL) *error = CSV_SUCCESS;
    return data;
    
    fail:
        free(data);
        return NULL;
    
    early_stop:
        return NULL;
}

/*******************************************************************************
Missing values lead to error rather than copying the nul-char as the target.
*/

char *csv_rowc(struct csv *csv, const uint32_t i, csv_errno *error)
{
    if (csv == NULL) STOP(error, CSV_NULL_INPUT_POINTER, early_stop);
    if (i >= csv->rows) STOP(error, CSV_PARAM_OUT_OF_BOUNDS, early_stop);
    
    char *data = malloc(sizeof(char) * csv->cols);
    if (data == NULL) STOP(error, CSV_MALLOC_FAILED, early_stop);
    
    for (uint32_t j = 0; j < csv->cols; j++)
    {
        char tmp = csv->data[i][j][0];
        
        if (tmp == '\0')
        {
            STOP(error, CSV_MISSING_DATA, fail);
        }
        else
        {
            data[j] = tmp;
        }
    }
    
    if (error != NULL) *error = CSV_SUCCESS;
    return data;
    
    fail:
        free(data);
        return NULL;
    
    early_stop:
        return NULL;
}

/*******************************************************************************
Missing values lead to error rather than copying the nul-char as the target.
*/

char *csv_colc(struct csv *csv, const uint32_t j, csv_errno *error)
{
    if (csv == NULL) STOP(error, CSV_NULL_INPUT_POINTER, early_stop);
    if (j >= csv->rows) STOP(error, CSV_PARAM_OUT_OF_BOUNDS, early_stop);
    
    char *data = malloc(sizeof(char) * csv->rows);
    if (data == NULL) STOP(error, CSV_MALLOC_FAILED, early_stop);
    
    for (uint32_t i = 0; i < csv->rows; i++)
    {
        char tmp = csv->data[i][j][0];
        
        if (tmp == '\0')
        {
            STOP(error, CSV_MISSING_DATA, fail);
        }
        else
        {
            data[i] = tmp;
        }
    }
    
    if (error != NULL) *error = CSV_SUCCESS;
    return data;
    
    fail:
        free(data);
        return NULL;
    
    early_stop:
        return NULL;
}

/******************************************************************************/

double *csv_rowd(struct csv *csv, const uint32_t i, csv_errno *error)
{
    if (csv == NULL) STOP(error, CSV_NULL_INPUT_POINTER, early_stop);
    if (i >= csv->rows) STOP(error, CSV_PARAM_OUT_OF_BOUNDS, early_stop);
    
    double *data = malloc(sizeof(double) * csv->cols);
    if (data == NULL) STOP(error, CSV_MALLOC_FAILED, early_stop);

    for (uint32_t j = 0; j < csv->cols; j++)
    {
        errno = 0;
        double tmp = 0;
        char *end = NULL;
        
        if (csv->data[i][j][0] == '\0') STOP(error, CSV_MISSING_DATA, fail);
        else tmp = strtod(csv->data[i][j], &end);
        
        //parse error handling
        if (end == csv->data[i][j]) 
            STOP(error, CSV_READ_FAIL, fail);
        if (errno == 0 && *end != '\0')
            STOP(error, CSV_READ_PARTIAL, fail);
        if (errno != 0)
            STOP(error, CSV_UNKNOWN_FATAL_ERROR, fail);
        
        //success
        assert(*end == '\0' && "missing condition");
        data[j] = tmp;
    }
    
    if (error != NULL) *error = CSV_SUCCESS;
    return data;
    
    fail:
        free(data);
        return NULL;
    
    early_stop:
        return NULL;
}

/******************************************************************************/

double *csv_cold(struct csv *csv, const uint32_t j, csv_errno *error)
{
    if (csv == NULL) STOP(error, CSV_NULL_INPUT_POINTER, early_stop);
    if (j >= csv->cols) STOP(error, CSV_PARAM_OUT_OF_BOUNDS, early_stop);
    
    double *data = malloc(sizeof(double) * csv->rows);
    if (data == NULL) STOP(error, CSV_MALLOC_FAILED, early_stop);

    for (uint32_t i = 0; i < csv->rows; i++)
    {
        errno = 0;
        double tmp = 0;
        char *end = NULL;
        
        if (csv->data[i][j][0] == '\0') STOP(error, CSV_MISSING_DATA, fail);
        else tmp = strtod(csv->data[i][j], &end);
        
        //parse error handling
        if (end == csv->data[i][j]) 
            STOP(error, CSV_READ_FAIL, fail);
        if (errno == 0 && *end != '\0')
            STOP(error, CSV_READ_PARTIAL, fail);
        if (errno != 0)
            STOP(error, CSV_UNKNOWN_FATAL_ERROR, fail);
        
        //success
        assert(*end == '\0' && "missing condition");
        data[i] = tmp;
    }
    
    if (error != NULL) *error = CSV_SUCCESS;
    return data;
    
    fail:
        free(data);
        return NULL;
    
    early_stop:
        return NULL;
}

/******************************************************************************/

const char *csv_errno_decode(const csv_errno error)
{
    switch (error)
    {
        case CSV_SUCCESS:
            return "successful API call.\n";
        case CSV_NULL_FILENAME:
            return "the provide filename pointer is null.\n";
        case CSV_INVALID_FILE:
            return "the provided filename is invalid.\n";
        case CSV_NUM_COLUMNS_OVERFLOW:
            return "the number of columns in the file exceeds UINT32_MAX.\n";
        case CSV_NUM_ROWS_OVERFLOW:
            return "the number of rows in the file exceeds UINT32_MAX.\n";
        case CSV_FIELD_LEN_OVERFLOW:
            return "attempted to parse a field that exceeds UINT32_MAX chars.\n";
        case CSV_BUFFER_OVERFLOW:
            return "the temporary buffer is not large enough to hold some field.\n";
        case CSV_MALLOC_FAILED:
            return "attempted stdlib malloc call has failed.\n";
        case CSV_FATAL_UNGETC:
            return "attempted stdio ungetc call has failed.\n";
        case CSV_NULL_INPUT_POINTER:
            return "provided input pointer is invalid.\n";
        case CSV_PARAM_OUT_OF_BOUNDS:
            return "provided input argument to function call is too large.\n";
        case CSV_UNKNOWN_FATAL_ERROR:
            return "fatal bug in csv.c has occured.\n";
        case CSV_READ_FAIL:
            return "field cannot be converted to requested type.\n";
        case CSV_READ_OVERFLOW:
            return "field is too large, errno.h errno is set to ERANGE.\n";
        case CSV_READ_UNDERFLOW:
            return "field is too small, errno.h errno is set to ERANGE.\n";
        case CSV_READ_PARTIAL:
            return "field was partially read during type conversion.\n";
        case CSV_INVALID_BASE:
            return "conversion to integer type failed, check base argument.\n";
        case CSV_MISSING_DATA:
            return "attempted to convert data at field, but none exists.\n";
        case CSV_UNDEFINED:
            return "error code has not been set.\ns";
    }
    
    return "unknown error code received.\n";
}
