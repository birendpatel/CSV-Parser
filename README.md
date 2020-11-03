# CSV Parser

RFC 4180 Compliant CSV parser for C applications. The API in src/csv.h contains the full documentation.

## Demo

```C
#include <stdio.h>
#include <stdlib.h>
#include "csv.h"

int main(void)
{
    //error handle uses enum csv_errno from csv.h
    csv_errno error = CSV_UNDEFINED;
    
    //read the contents of a RFC 4180 compliant file into memory.
    struct csv *csv = csv_read("data.csv", true, &error);
    
    //all API functions return CSV_SUCCESS on success
    if (error != CSV_SUCCESS) fprintf(stderr, "%s", csv_errno_decode(error));
    
    //the struct is transparent so you can access the header and field metadata
    for (size_t i = 0; i < csv->cols; i++) puts(csv->headers[i]);
    printf("total missing fields: %d\n", csv->missing);
    printf("total rows: %d\n", csv->rows);
    printf("total cols: %d\n", csv->cols);
    
    //all values are stored in a 2D array of null-terminated strings
    puts(csv->data[3][4]);
    puts(csv->data[0][0]);
    puts(csv->data[csv->rows - 1][csv->cols - 1]);
    
    //but you can copy rows or columns of homogenous data to the type you need.
    //for example, pack a row of long int values into a standard C array
    long *data_in_first_row = csv_rowl(csv, 0, 10, &error);
    if (error != CSV_SUCCESS) fprintf(stderr, "%s", csv_errno_decode(error));
    
    //or pack a column of chars into another standard C array
    char *data_in_final_column = csv_colc(csv, csv->cols - 1, &error);
    if (error != CSV_SUCCESS) fprintf(stderr, "%s", csv_errno_decode(error));
    
    //its the user's responsibility to free arrays returned by the packing functions
    free(data_in_first_row);
    free(data_in_final_column);
    
    //csv_read() allocates a great deal of memory. Dont forget to free it!
    csv_free(csv);
    
    return EXIT_SUCCESS;
}
```
