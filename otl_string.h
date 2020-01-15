//
// Created by hsyuan on 2019-03-11.
//

#ifndef PROJECT_BM_STRING_H
#define PROJECT_BM_STRING_H

#include <vector>

class String {
public:
    static std::vector<std::string> split(std::string str, std::string pattern)
    {
        std::string::size_type pos;
        std::vector<std::string> result;
        str += pattern;
        size_t size = str.size();

        for (size_t i = 0; i < size; i++)
        {
            pos = str.find(pattern, i);
            if (pos < size)
            {
                std::string s = str.substr(i, pos - i);
                result.push_back(s);
                i = pos + pattern.size() - 1;
            }
        }
        return result;
    }

    static uint32_t gen_random_uint(uint32_t min, uint32_t max);

    static const std::string gen_random_string(size_t len)
    {
        static char buffer[64];
        static const char chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b',
                                      'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                      'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z' };

        if (len > 64)
            len = 64;

        for (size_t i{ 0 }; i < len; ++i)
            buffer[i] = chars[gen_random_uint(0, sizeof(chars) - 1)];

        return std::string(buffer, len);
    }

    static bool start_with(const std::string &str, const std::string &head)
    {
        return str.compare(0, head.size(), head) == 0;
    }

    static std::string file_name_from_path(const std::string& path, bool hasExt){
        int pos = path.find_last_of('/');
        std::string str = path.substr(pos+1, path.size());
        if (!hasExt) {
            pos = str.find_last_of('.');
            if (std::string::npos != pos) {
                str = str.substr(0, pos);
            }
        }
        return str;
    }

    static std::string file_ext_from_path(const std::string& str){
        std::string ext;
        auto pos = str.find_last_of('.');
        if (std::string::npos != pos) {
            ext = str.substr(0, pos);
        }
        return ext;
    }

    static std::string format(const char *pszFmt, ...)
    {
        std::string str;
        va_list args, args2;
        va_start(args, pszFmt);
        {
            va_copy(args2, args);
            int nLength = vsnprintf(nullptr, 0, pszFmt, args2);
            nLength += 1;  //上面返回的长度是包含\0，这里加上
            std::vector<char> vectorChars(nLength);
            vsnprintf(vectorChars.data(), nLength, pszFmt, args);
            str.assign(vectorChars.data());
        }
        va_end(args);
        return str;
    }

};


#endif //PROJECT_BM_STRING_H
