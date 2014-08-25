/****************************** Module Header ******************************\
* Module Name:  CppSparseFile.cpp
* Project:      CppSparseFile
* Copyright (c) Microsoft Corporation.
*
* CppSparseFile demonstrates the common operations on sparse files. A sparse
* file is a type of computer file that attempts to use file system space more
* efficiently when blocks allocated to the file are mostly empty. This is
* achieved by writing brief information (metadata) representing the empty
* blocks to disk instead of the actual "empty" space which makes up the
* block, using less disk space. You can find in this example the creation of
* sparse file, the detection of sparse attribute, the retrieval of sparse
* file size, and the query of sparse file layout.
*
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/opensource/licenses.mspx#Ms-PL.
* All other rights reserved.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma region Includes
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <assert.h>
#pragma endregion


/*!
* VolumeSupportsSparseFiles determines if the volume supports sparse streams.
*
* \param lpRootPathName
* Volume root path e.g. C:\
*/
BOOL VolumeSupportsSparseFiles(LPCTSTR lpRootPathName);


/*!
* IsSparseFile determines if a file is sparse.
*
* \param lpFileName
* File name
*/
BOOL IsSparseFile(LPCTSTR lpFileName);

/*!
* Get sparse file sizes.
*
* \param lpFileName
* File name
*
* \see
* http://msdn.microsoft.com/en-us/library/aa365276.aspx
*/
BOOL GetSparseFileSize(LPCTSTR lpFileName);


/*!
* Create a sparse file.
*
* \param lpFileName
* The name of the sparse file
*/
HANDLE CreateSparseFile(LPCTSTR lpFileName);

/*!
* Converting a file region to A sparse zero area.
*
* \param hSparseFile
* Handle of the sparse file
*
* \param start
* Start address of the sparse zero area
*
* \param size
* Size of the sparse zero block. The minimum sparse size is 64KB.
*
* \remarks
* Note that SetSparseRange does not perform actual file I/O, and unlike the
* WriteFile function, it does not move the current file I/O pointer or sets
* the end-of-file pointer. That is, if you want to place a sparse zero block
* in the end of the file, you must move the file pointer accordingly using
* the FileStream.Seek function, otherwise DeviceIoControl will have no effect
*/
void SetSparseRange(HANDLE hSparseFile, LONGLONG start, LONGLONG size);

/*!
* Query the sparse file layout.
*
* \param lpFileName
* File name
*/
BOOL GetSparseRanges(LPCTSTR lpFileName);