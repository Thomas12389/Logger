
#include <unistd.h>
#include "../tfm_common.h"
#include "get_version.h"

#include "../INI_Parse/INIReader.h"

int version_str_to_num(const std::string ver_str, VERSION_INFO *ver_info) {
    size_t s_pos = 0;
    // maj_version
    size_t e_pos = ver_str.find_first_of(".", s_pos);
    if (std::string::npos == e_pos) {
        return -1;
    }
    std::string sub_str = ver_str.substr(s_pos, e_pos - s_pos);
    ver_info->maj_version = std::strtoul(sub_str.c_str(), NULL, 10);
    s_pos = e_pos + 1;
    // min_version
    e_pos = ver_str.find_first_of(".", s_pos);
    if (std::string::npos == e_pos) {
        return -1;
    }
    sub_str = ver_str.substr(s_pos, e_pos - s_pos);
    ver_info->min_version = std::strtoul(sub_str.c_str(), NULL, 10);
    s_pos = e_pos + 1;
    // revision_number
    e_pos = ver_str.find_first_of(".", s_pos);
    if (std::string::npos == e_pos) {
        sub_str = ver_str.substr(s_pos, ver_str.length() - s_pos);
        ver_info->revision_number = std::strtoul(sub_str.c_str(), NULL, 10);
        ver_info->reserved = 0;
    } else {
        sub_str = ver_str.substr(s_pos, e_pos - s_pos);
        ver_info->revision_number = std::strtoul(sub_str.c_str(), NULL, 10);
        s_pos = e_pos + 1;
        sub_str = ver_str.substr(s_pos, ver_str.length() - s_pos);
        ver_info->reserved = std::strtoul(sub_str.c_str(), NULL, 10);
    }

    return 0;
}

int get_wcp_version(VERSION_INFO *ver_info) {
    if (0 != access(WCP_INFO_INI_PATH.c_str(), F_OK)) {
        return version_str_to_num("0.0.0", ver_info);
    }

    INIReader reader(WCP_INFO_INI_PATH);
    if (reader.ParseError() < 0) {
        return -1;
    }

    const std::string section = "info";
    const std::string key = "version";
    if (false == reader.HasValue(section, key)) { 
        return -1; 
    }

    std::string wcp_version_str = reader.GetString(section, key, "0.0.0");
    return version_str_to_num(wcp_version_str, ver_info);
}

int get_tcp_version(VERSION_INFO *ver_info) {
    INIReader reader(TCP_INFO_INI_PATH);
    if (reader.ParseError() < 0) {
        return -1;
    }

    const std::string section = "info";
    const std::string key = "version";
    if (false == reader.HasValue(section, key)) { 
        return -1; 
    }

    std::string tcp_version_str = reader.GetString(section, key, "0.0.0");
    return version_str_to_num(tcp_version_str, ver_info);
}

// #include <regex.h>
int get_sys_fireware_version(VERSION_INFO *ver_info) {

    char buff[256] = {0};
    FILE *pFILE = fopen("/etc/issue.owa", "r");
    if (NULL == pFILE) {
        return -1;
    }

    if (NULL == fgets(buff, sizeof(buff), pFILE)) {
		fclose(pFILE);
		return -1;
	}

    char version[10] = {0};
    /* %*[^V]   匹配不含 V 的最长字符串，但不捕获
     * %*[V]    匹配 V ，但不捕获
     * %[0-9.]  匹配只含有 0-9 及 . 的字符串
     */
    int cnt = sscanf(buff, "%*[^V]%*[V]%[0-9.]", version);
    DEBUG_PTINTF("{%s}\n", version);
    if (1 != cnt) {
        fclose(pFILE);
        return -1;
    }

// const size_t nmatch = 1;
// regmatch_t pmatch[1];
// int cflags = REG_EXTENDED;
// regex_t reg;
// const char *pattern = "\\d+(\\.\\d+){0,2}";

// regcomp(&reg, pattern, cflags);
// int status = regexec(&reg, buff, nmatch, pmatch, 0);
// if (status == REG_NOMATCH) {
//     DEBUG_PTINTF("No match\n");
// } else if (REG_NOERROR == status) {
//     DEBUG_PTINTF("%d, %d\n", pmatch[0].rm_so, pmatch[0].rm_eo);
//     for (int i = pmatch[0].rm_so; i < pmatch[0].rm_eo; ++i) {
//         putchar(buff[i]);
//     }
// }
// regfree(&reg);
    std::string sysfw_version_str = version;

    fclose(pFILE);
    return version_str_to_num(sysfw_version_str, ver_info);
}

