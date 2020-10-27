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
static int csv_get_header(struct csv *csv, FILE * const csvfile);

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
#define SUCCESS 1
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
        status = csv_get_header(csv, csvfile);
        if (status != SUCCESS) STOP(error, status, fail);
    }
    
    //read each datum into a struct csv_cell
    
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
If the CSV file contains a header, dynamically allocate a ragged array of column
names and assign to struct csv metadata. By RFC 4180 rule 3, the header has the
same format as the records.
*/

static int csv_get_header(struct csv *csv, FILE * const csvfile)
{
    rewind(csvfile);
    
    uint32_t i = 0;
    int c = 0;
    uint64_t lag = 0;
    uint64_t lead = 0;
    char *tmp = NULL;
    
    csv->header = malloc(sizeof(void*) * (uint64_t) csv->rows);
    if (csv->header == NULL) return CSV_MALLOC_FAILED;
    
    while (i != csv->cols)
    {
        c = getc(csvfile);
        
        switch (c)
        {
            //quoted fields
            case '"':
                lag++;
                lead++;
                
                //advance lead to consume all chars incl. escape sequences
                while (1)
                {
                    c = getc(csvfile);
                    
                    if (c == '"')
                    {
                        c = getc(csvfile);
                        
                        //else advances by two to consume both quotes
                        if (c == ',' || c == '\n') break;
                        else lead += 2;
                    }
                    else lead++;
                }
                
                //identical to default, just copy contents to a temp space
                tmp = malloc(sizeof(char) * (lead - lag) + 1);
                if (tmp == NULL) return CSV_MALLOC_FAILED;
                
                fsetpos(csvfile, (const fpos_t *) &lag);
                fread(tmp, sizeof(char), lead - lag, csvfile);
                tmp[lead - lag] = '\0';
                
                //unlike default, need to transform quote pairs to single quotes
                char *tmp_2 = malloc(sizeof(char) * (lead - lag) + 1);
                if (tmp_2 == NULL) return CSV_MALLOC_FAILED;
                
                for (uint64_t j = 0, k = 0; j < strlen(tmp) + 1; j++, k++)
                {
                    if (tmp[j] == '"' && tmp[j+1] == '"')
                    {
                        tmp_2[k] = '"';
                        j++;
                    }
                    else tmp_2[k] = tmp[j];
                }
                
                //shrink array to remove bytes once held by the escape quotes
                free(tmp);
                tmp = malloc(sizeof(char) * strlen(tmp_2) + 1);
                if (tmp == NULL) return CSV_MALLOC_FAILED;
                memcpy(tmp, tmp_2, strlen(tmp_2) + 1);
                free(tmp_2);
                csv->header[i] = tmp;
                
                //reset lag, lead, and file pos at next column.
                lead += 2;
                fsetpos(csvfile, (const fpos_t *) &lead);
                lag = lead;
                
                break;
            
            //unquoted fields
            default:
                while (!(c == ',' || c == '\n')) 
                {
                    lead++;
                    c = getc(csvfile);
                }
                
                tmp = malloc(sizeof(char) * (lead - lag) + 1);
                if (tmp == NULL) return CSV_MALLOC_FAILED;
                
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
