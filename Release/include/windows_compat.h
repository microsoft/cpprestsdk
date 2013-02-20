#pragma once

#include <safeint.h>
#include <string>

#if _MSC_VER >= 1700
// Support VS2012 SAL syntax only
#include <sal.h>
#else
#include "nosal.h"
#endif

#define _noexcept 

typedef wchar_t utf16char;
typedef std::wstring utf16string;
typedef std::wstringstream utf16stringstream;
typedef std::wostringstream utf16ostringstream;
typedef std::wostream utf16ostream;
typedef std::wistream utf16istream;
typedef std::wistringstream utf16istringstream;

#define U(x) L ## x

typedef msl::utilities::SafeInt<size_t> SafeSize;

