
#ifndef __GET_VERSION_H__
#define __GET_VERSION_H__

#define VERSION_LEN     28

typedef struct VERSION_INFO {
	uint8_t maj_version;
	uint8_t min_version;
	uint8_t revision_number;
	uint8_t reserved;
    // 重载 < 运算符
    bool operator <(const VERSION_INFO& t) const {
        if (maj_version == t.maj_version) {
            if (min_version == t.min_version) {
                return (revision_number < t.revision_number);
            }
            
            return (min_version < t.min_version);
        }
        
        return (maj_version < t.maj_version);
    }
} VERSION_INFO;

extern int get_sys_fireware_version(VERSION_INFO *ver_info);
extern int get_tcp_version(VERSION_INFO *ver_info);
extern int get_wcp_version(VERSION_INFO *ver_info);

#endif
