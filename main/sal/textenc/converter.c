/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#include "converter.h"
#include "tenchelp.h"
#include "unichars.h"
#include "rtl/textcvt.h"
#include "sal/types.h"

ImplBadInputConversionAction ImplHandleBadInputTextToUnicodeConversion(
    sal_Bool bUndefined, sal_Bool bMultiByte, sal_Char cByte, sal_uInt32 nFlags,
    sal_Unicode ** pDestBufPtr, sal_Unicode * pDestBufEnd, sal_uInt32 * pInfo)
{
    *pInfo |= bUndefined
        ? (bMultiByte
           ? RTL_TEXTTOUNICODE_INFO_MBUNDEFINED
           : RTL_TEXTTOUNICODE_INFO_UNDEFINED)
        : RTL_TEXTTOUNICODE_INFO_INVALID;
    switch (nFlags
            & (bUndefined
               ? (bMultiByte
                  ? RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_MASK
                  : RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_MASK)
               : RTL_TEXTTOUNICODE_FLAGS_INVALID_MASK))
    {
    case RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_ERROR:
    case RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_ERROR:
    case RTL_TEXTTOUNICODE_FLAGS_INVALID_ERROR:
        *pInfo |= RTL_TEXTTOUNICODE_INFO_ERROR;
        return IMPL_BAD_INPUT_STOP;

    case RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_IGNORE:
    case RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_IGNORE:
    case RTL_TEXTTOUNICODE_FLAGS_INVALID_IGNORE:
        return IMPL_BAD_INPUT_CONTINUE;

    case RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_MAPTOPRIVATE:
        if (*pDestBufPtr != pDestBufEnd)
        {
            *(*pDestBufPtr)++ = RTL_TEXTCVT_BYTE_PRIVATE_START
                | ((sal_uChar) cByte);
            return IMPL_BAD_INPUT_CONTINUE;
        }
        else
            return IMPL_BAD_INPUT_NO_OUTPUT;

    default: /* RTL_TEXTTOUNICODE_FLAGS_UNDEFINED_DEFAULT,
                RTL_TEXTTOUNICODE_FLAGS_MBUNDEFINED_DEFAULT,
                RTL_TEXTTOUNICODE_FLAGS_INVALID_DEFAULT */
        if (*pDestBufPtr != pDestBufEnd)
        {
            *(*pDestBufPtr)++ = RTL_TEXTENC_UNICODE_REPLACEMENT_CHARACTER;
            return IMPL_BAD_INPUT_CONTINUE;
        }
        else
            return IMPL_BAD_INPUT_NO_OUTPUT;
    }
}

ImplBadInputConversionAction
ImplHandleBadInputUnicodeToTextConversion(sal_Bool bUndefined,
                                          sal_uInt32 nUtf32,
                                          sal_uInt32 nFlags,
                                          sal_Char ** pDestBufPtr,
                                          sal_Char * pDestBufEnd,
                                          sal_uInt32 * pInfo,
                                          sal_Char const * pPrefix,
                                          sal_Size nPrefixLen,
                                          sal_Bool * pPrefixWritten)
{
    /* TODO! RTL_UNICODETOTEXT_FLAGS_UNDEFINED_REPLACE
             RTL_UNICODETOTEXT_FLAGS_UNDEFINED_REPLACESTR */

    sal_Char cReplace;

    if (bUndefined)
    {
        if (ImplIsControlOrFormat(nUtf32))
        {
            if ((nFlags & RTL_UNICODETOTEXT_FLAGS_CONTROL_IGNORE) != 0)
                nFlags = RTL_UNICODETOTEXT_FLAGS_UNDEFINED_IGNORE;
        }
        else if (ImplIsPrivateUse(nUtf32))
        {
            if ((nFlags & RTL_UNICODETOTEXT_FLAGS_PRIVATE_IGNORE) != 0)
                nFlags = RTL_UNICODETOTEXT_FLAGS_UNDEFINED_IGNORE;
            else if ((nFlags & RTL_UNICODETOTEXT_FLAGS_PRIVATE_MAPTO0) != 0)
                nFlags = RTL_UNICODETOTEXT_FLAGS_UNDEFINED_0;
        }
        else if (ImplIsZeroWidth(nUtf32))
        {
            if ((nFlags & RTL_UNICODETOTEXT_FLAGS_NONSPACING_IGNORE) != 0)
                nFlags = RTL_UNICODETOTEXT_FLAGS_UNDEFINED_IGNORE;
        }
    }
    *pInfo |= bUndefined ? RTL_UNICODETOTEXT_INFO_UNDEFINED :
                           RTL_UNICODETOTEXT_INFO_INVALID;
    switch (nFlags & (bUndefined ? RTL_UNICODETOTEXT_FLAGS_UNDEFINED_MASK :
                                   RTL_UNICODETOTEXT_FLAGS_INVALID_MASK))
    {
    case RTL_UNICODETOTEXT_FLAGS_UNDEFINED_ERROR:
    case RTL_UNICODETOTEXT_FLAGS_INVALID_ERROR:
        *pInfo |= RTL_UNICODETOTEXT_INFO_ERROR;
        return IMPL_BAD_INPUT_STOP;

    case RTL_UNICODETOTEXT_FLAGS_UNDEFINED_IGNORE:
    case RTL_UNICODETOTEXT_FLAGS_INVALID_IGNORE:
        if (pPrefixWritten)
            *pPrefixWritten = sal_False;
        return IMPL_BAD_INPUT_CONTINUE;

    case RTL_UNICODETOTEXT_FLAGS_UNDEFINED_0:
    case RTL_UNICODETOTEXT_FLAGS_INVALID_0:
        cReplace = 0;
        break;

    case RTL_UNICODETOTEXT_FLAGS_UNDEFINED_QUESTIONMARK:
    case RTL_UNICODETOTEXT_FLAGS_INVALID_QUESTIONMARK:
    default: /* RTL_UNICODETOTEXT_FLAGS_UNDEFINED_DEFAULT,
                RTL_UNICODETOTEXT_FLAGS_INVALID_DEFAULT */
        cReplace = '?';
        break;

    case RTL_UNICODETOTEXT_FLAGS_UNDEFINED_UNDERLINE:
    case RTL_UNICODETOTEXT_FLAGS_INVALID_UNDERLINE:
        cReplace = '_';
        break;
    }
    if ((sal_Size) (pDestBufEnd - *pDestBufPtr) > nPrefixLen)
    {
        while (nPrefixLen-- > 0)
            *(*pDestBufPtr)++ = *pPrefix++;
        *(*pDestBufPtr)++ = cReplace;
        if (pPrefixWritten)
            *pPrefixWritten = sal_True;
        return IMPL_BAD_INPUT_CONTINUE;
    }
    else
        return IMPL_BAD_INPUT_NO_OUTPUT;
}
