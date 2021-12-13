#ifndef __PARSEXMLCAN_OBD_H__
#define __PARSEXMLCAN_OBD_H__

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "DevConfig/OBDMessageStruct.h"
#include <map>

// OBD
#ifdef SELF_OBD
typedef std::map<unsigned int, OBD_RecvPackage*> OBD_MAP_MsgID_RMsgPkg;    // unsigned int -- Message ID
int parse_xml_obd(const char *ChanName, int nNumFunctions);
#else
int parse_xml_obd(rapidxml::xml_node<> *pOBDNode, const char *ChanName, const char *pOBDType, int nNumSignals);
typedef std::map<std::string, OBD_Message*> OBD_MAP_Type_Msg;		// string -- OBD or WWHOBD or J1939
#endif

#endif