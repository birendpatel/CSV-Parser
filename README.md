# CSV Parser

RFC 4180 compliant CSV parser for C99+ applications. The API in src/csv.h contains the full documentation. This parser trades off computational and memory efficiency in favor of simplicity and ease-of-use. The csv data is held in memory as a 3D ragged array of strings. For some problem domains, such as data analytics where columnar data slicing is frequently performed, a great deal of cache thrashing will occur. But, for prototyping, CLI tools, and small/medium dataset processing this should get the job done.

## Demo

```C
#include <stdio.h>
#include <stdlib.h>
#include "csv.h"

int main(void)
{
    //optional error handle used by all API functions
    csv_errno error = CSV_UNDEFINED;
    
    //read the contents of a RFC 4180 compliant file into memory.
    //if you're not handling errors, just pass NULL instead.
    struct csv *csv = csv_read("data.csv", true, &error);
    
    //all API functions set error to CSV_SUCCESS (zero) on success
    if (error != CSV_SUCCESS) fprintf(stderr, "%s", csv_errno_decode(error));
    
    //access the header and field metadata through the transparent struct
    for (size_t i = 0; i < csv->cols; i++) puts(csv->headers[i]);
    printf("total missing fields: %d\n", csv->missing);
    printf("total rows: %d\n", csv->rows);
    printf("total cols: %d\n", csv->cols);
    
    //all fields except the header row are stored in the data member
    puts(csv->data[3][4]);
    puts(csv->data[0][0]);
    puts(csv->data[csv->rows - 1][csv->cols - 1]);
    
    //you can copy rows or columns of homogenous data to the type you need.
    //for example, pack a row of long int values into a standard C array.
    long *data_in_first_row = csv_rowl(csv, 0, 10, &error);
    if (error != CSV_SUCCESS) fprintf(stderr, "%s", csv_errno_decode(error));
    
    //or pack a column of chars into another standard C array
    char *data_in_final_column = csv_colc(csv, csv->cols - 1, &error);
    if (error != CSV_SUCCESS) fprintf(stderr, "%s", csv_errno_decode(error));
    
    //its the user's responsibility to free arrays returned by the packing functions
    free(data_in_first_row);
    free(data_in_final_column);
    
    //csv_read() allocates quite a bit of memory. Dont forget to free it!
    csv_free(csv);
    
    return EXIT_SUCCESS;
}
```
