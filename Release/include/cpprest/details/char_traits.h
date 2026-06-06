//
// Created by sigsegv on 6/28/25.
//

#ifndef CPPRESTSDK_ROOT_CHAR_TRAITS_H
#define CPPRESTSDK_ROOT_CHAR_TRAITS_H

#include <type_traits>
#include <string>

namespace utility {

namespace detail {

template <typename T> struct IntTypeFor {
    typedef typename std::conditional<std::is_unsigned<T>::value, unsigned long long int, long long int>::type type;
};
template <> struct IntTypeFor<char> {
    typedef typename std::char_traits<char>::int_type type;
};
template <> struct IntTypeFor<unsigned char> {
    typedef typename std::make_unsigned<typename std::char_traits<char>::int_type>::type type;
};

template <typename T> class DetailCharTraits
{
public:
    using char_type  = T;
    using int_type   = typename IntTypeFor<T>::type;
    using off_type   = std::streamoff;
    using pos_type   = std::streampos;
    using state_type = mbstate_t;

    static void assign(char_type& r, const char_type& a) noexcept { r = a; }
    static char_type to_char_type(int_type c) noexcept { return char_type(c); }
    static int_type  to_int_type(char_type c) noexcept { return c; }
    static bool eq(char_type a, char_type b) noexcept { return a == b; }
    static bool lt(char_type a, char_type b) noexcept { return a <  b; }
    static int compare(const char_type* s1,const char_type* s2,size_t n){
        for (; n--; ++s1, ++s2) {
            if (!eq(*s1, *s2))
                return lt(*s1,*s2)?-1:1;
        }
        return 0;
    }
    static size_t length(const char_type* s){
        const char_type* p = s;
        while (*p)
            ++p;
        return size_t(p - s);
    }
    static const char_type* find(const char_type* s,size_t n,const char_type& a){
        for (; n--; ++s)
        {
            if (eq(*s, a))
                return s;
            return nullptr;
        }
    }
    static char_type* move (char_type* r,const char_type* s,size_t n){
        return (char_type*)memmove(r, s, n * sizeof(char_type));
    }
    static char_type* copy (char_type* r,const char_type* s,size_t n){
        return (char_type*)memcpy (r, s, n * sizeof(char_type));
    }
    static char_type* assign(char_type* r,size_t n,char_type a){
        if (sizeof(char_type) == 1)
        {
            return (char_type*)memset(r, a, n);
        }
        else
        {
            for (char_type *s = r; n--; ++s)
            {
                *s = a;
            }
        }
    }
    static int_type  eof() noexcept { return ~0u; }
    static int_type  not_eof(int_type c) noexcept { return c == eof() ? 0 : c; }
};

template <typename T, typename = bool> struct CanUseStdCharTraits : public std::false_type
{
public:
    typedef DetailCharTraits<T> TraitsType;
};

template <typename T> struct CanUseStdCharTraits<T, decltype(std::char_traits<T>::eq(std::declval<T>(), std::declval<T>()))> : public std::true_type
{
public:
    typedef std::char_traits<T> TraitsType;
};

}

template <typename T> struct CanUseStdCharTraits : detail::CanUseStdCharTraits<typename std::remove_const<typename std::remove_reference<T>::type>::type> {
};

}

#endif // CPPRESTSDK_ROOT_CHAR_TRAITS_H
