#ifndef HELPERH
#define HELPERH


#include <ctime>
#include <cstring>
#include <cassert>

#include <iostream>
#include <string>
#include <sstream>

#include <iomanip>


#define assertm(exp, msg) assert(((void)msg, exp))


template <typename T>
std::string tohex(T t)
{
    // std::format("{:x}", 123);
    std::stringstream sstream;
    sstream << std::hex << t;
    return sstream.str();
}

uint64_t fromhex(const std::string& str)
{
    std::istringstream converter(str);
    uint64_t value = 0;
    converter >> std::hex >> value;
    return value;
}

template <typename T>
std::string to_string(T t)
{
    // std::format("{:x}", 123);
    std::stringstream sstream;
    sstream << std::dec << t;
    return sstream.str();
}



/*
some helper functions
note: PIN more or less supports c++11
*/

/*
//
// no fold expressions in pin yet...
//

template<typename S, typename T>
void print_aligned(S& s, int width, T&& t)
{
    s << std::setw(width) << t;
}

template<typename S, size_t N, typename... Args>
void print_aligned(S& s, const int (&a)[N], Args&&... args)
{
    static_assert(N >= sizeof...(args), "array of widths must match number of parameters");
    int i = 0;
    (print_aligned(s, a[i++], std::forward<Args>(args)), ...);
    s << std::endl;
}

template<size_t N, typename... Args>
void print_aligned(const int (&a)[N], Args&&... args)
{
    print_aligned(std::cout, a, std::forward<Args>(args)...);
}
*/

template <typename S, size_t N, typename T>
void print_aligned_(S &s, const int (&a)[N], const size_t i, T &&t)
{
    assertm(N >= i, "static array out of bound");
    s << std::setw(a[i]) << t << " ";
}

template <typename S, size_t N, typename T, typename... Args>
void print_aligned_(S &s, const int (&a)[N], const size_t i, T &&t, Args &&...args)
{
    static_assert(N >= sizeof...(args), "array of widths must match number of parameters");
    print_aligned_(s, a, i, t);
    print_aligned_(s, a, i + 1, args...);
}

template <typename Stream, size_t N, typename... Args>
void print_aligned(Stream &s, const int (&a)[N], Args &&...args)
{
    print_aligned_(s, a, 0, args...);
    s << std::endl;
}

template <size_t N, typename... Args>
void print_aligned(const int (&a)[N], Args &&...args)
{
    print_aligned_(std::cout, a, args...);
    std::cout << std::endl;
}


const char *StripPath(const char *path)
{
    const char *file = strrchr(path, '/');
    if(file)
        return file + 1;
    file = strrchr(path, '\\');
    if (file)
        return file + 1;
    return path;
}

constexpr int NumDigits(int x)
{  
    //x = std::abs(x); 
    if (x < 0)
        x *= -1;
    return (x < 10 ? 1 :   
        (x < 100 ? 2 :   
        (x < 1000 ? 3 :   
        (x < 10000 ? 4 :   
        (x < 100000 ? 5 :   
        (x < 1000000 ? 6 :   
        (x < 10000000 ? 7 :  
        (x < 100000000 ? 8 :  
        (x < 1000000000 ? 9 :  
        10)))))))));  
}

std::string datetimestring()
{
    std::time_t result = std::time(nullptr);
    return std::asctime(std::localtime(&result));
}

/*
 * Trim whitespace from a line of text.  Leading and trailing whitespace is removed.
 * Any internal whitespace is replaced with a single space (' ') character.
 *
 *  inLine[in]  Input text line.
 *
 * Returns: A string with the whitespace trimmed.
 */
std::string TrimWhitespace(const std::string &inLine)
{
    std::string outLine = inLine;

    bool skipNextSpace = true;
    for(std::string::iterator it = outLine.begin(); it != outLine.end(); ++it)
    {
        if(std::isspace(*it))
        {
            if(skipNextSpace)
            {
                it = outLine.erase(it);
                if(it == outLine.end())
                    break;
            }
            else
            {
                *it = ' ';
                skipNextSpace = true;
            }
        }
        else
        {
            skipNextSpace = false;
        }
    }
    if(!outLine.empty())
    {
        std::string::reverse_iterator it = outLine.rbegin();
        if(std::isspace(*it))
            outLine.erase(outLine.size() - 1);
    }
    return outLine;
}


#endif

