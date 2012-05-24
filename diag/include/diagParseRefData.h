/*
 * Copyright 2011 - 2012 Google Inc. All Rights Reserved.
 *
 * This file provides diagnostics monitoring related functions
 *
 */

#ifndef _DIAGD_REF_DATA_H_
#define _DIAGD_REF_DATA_H_

/* Diag reference data */
#define DIAGD_REF_DATA_FILE   "/user/diag/diag_ref_data.txt"

/*
 * Prototypes
 */
int  diagReadDiagDataFile(char *pFileName);
void diagGetDiagData(char *pDataBuf);
void diagSetDiagData(char *pClassName, char *pMemberName, char *pValue);
void diagSetUint32Value(char *pValue, UINT32 *pData);
void diagSetFloatValue(char *pValue, float *pData);

#endif // end of _DIAGD_REF_DATA_H_

