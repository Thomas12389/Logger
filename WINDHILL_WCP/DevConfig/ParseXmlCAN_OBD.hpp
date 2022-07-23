#ifndef __PARSEXMLCAN_OBD_H__
#define __PARSEXMLCAN_OBD_H__

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "DevConfig/OBDMessageStruct.h"
#include <map>

// first   uint32_t -- Message ID(Request)
// second   uint32_t -- Message ID(Response)
typedef std::map<uint32_t, std::map<uint32_t, OBD_Mode01RecvPackage*> > OBD_MAP_MsgID_Mode01_RMsgPkg;
int parse_xml_obd_package(rapidxml::xml_node<> *pOBDNode, const char *ChanName, int nNumPackages);

#endif