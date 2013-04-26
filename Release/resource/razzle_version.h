/**********************************************************************
* Microsoft (R) VCOMP
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

#include "verstamp.h"
#include "proj_version.h"

#define VER_FILETYPE                VFT_DLL
#define VER_FILESUBTYPE             VFT2_UNKNOWN
#define VER_FILEDESCRIPTION_STR     L"Microsoft\256 C++ REST SDK"
#ifdef VER_FILEOS
#undef VER_FILEOS
#endif
#define VER_FILEOS                  VOS_NT_WINDOWS32

#define VER_INTERNALNAME_STR        "Casablanca120.DLL"

#define VER_FILEVERSION             _CPPREST_RMJ,_CPPREST_RMM,_CPPREST_RUP,_CPPREST_RBLD
#define VER_FILEVERSION_STR         VERSION_STR2(_CPPREST_RMJ,_CPPREST_RMM,_CPPREST_RUP,_CPPREST_RBLD)


#include <common.ver>
