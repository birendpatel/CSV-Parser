/*
* NAME: Copyright (c) 2020, Biren Patel
* DESC: Unit tests for CSV parser
* LISC: MIT License
*/

#include <stdlib.h>
#include <stdbool.h>

#include "unity/unity.h"
#include "csv.h"

/******************************************************************************/

void setUp(void) {}
void tearDown(void) {}

/******************************************************************************/

int main(void)
{
    UNITY_BEGIN();
    UNITY_END();
    
    int status;
    struct csv *csv = csv_read("testfile.csv", true, &status);
    
    printf("error code: %d\n", status);
    printf("rows: %d\n", (int) csv->rows);
    printf("cols: %d\n", (int) csv->cols);
    printf("total: %d\n\n", (int) csv->total);
    
    char *x = csv_colc(csv, 0, &status);
    printf("error code: %d\n", status);
    
    for (int i = 0; i < (int) csv->rows; i++) printf("%c ", x[i]);
    puts("");
    
    csv_free(csv);
    puts("good free");
    
    return EXIT_SUCCESS;
}
