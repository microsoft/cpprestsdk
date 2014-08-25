/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* CppSparseFile.h : defines various apis for creation and access of sparse files under windows
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

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
