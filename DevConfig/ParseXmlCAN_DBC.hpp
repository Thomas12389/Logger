#ifndef __PARSEXMLCAN_DBC_H__
#define __PARSEXMLCAN_DBC_H__

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "DevConfig/DBCMessageStruct.h"
#include <map>


// DBC
typedef std::map<unsigned int, ReceiveMessagePack*> COM_MAP_MsgID_RMsgPkg;    // unsigned int -- Message ID

// DBC signal
int parse_xml_dbc_package(rapidxml::xml_node<> *pCOMNode, const char *ChanName, int nNumpkgs);
int parse_xml_dbc_signal(rapidxml::xml_node<> *pPackageNode, const char *ChanName);

#endif