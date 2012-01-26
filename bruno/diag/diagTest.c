/*
 * Copyright 2011 Google Inc. All Rights Reserved.
 *
 * This file is for development testing purpose.
 *
 * Ignore the implementation in this file
 *
 */

/*--------------------------------------------------------------------------
 *
 * Includes
 *
 *--------------------------------------------------------------------------
 */

#include "diagdIncludes.h"


/* -------------------------------------------------------------------------
 *
 * Internal defines, types and variables
 *
 *--------------------------------------------------------------------------
 */
#ifdef MOD_NAME
 #undef MOD_NAME
#endif
#define MOD_NAME  "diagTests\0"



/* -------------------------------------------------------------------------
 *
 * Routines
 *
 *--------------------------------------------------------------------------
 */


int main(void)
{
  char  c;

  diagd_Init();

  do {

    printf("q to quit, other to call diagMoca_MonServicePerf()....");
    c = getchar();
    printf("\n");

    if (c == 'q') {
      break;
    }
    else if (c != 'c')
      continue;

    /* Monitor MoCA service performance. */
    diagMoca_MonServicePerf();

    /* Check the error counts */
    diagMoca_MonErrorCounts();

  } while (true);

}
