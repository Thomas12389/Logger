#ifndef __PARSEXMLCAN_XCP_H__
#define __PARSEXMLCAN_XCP_H__

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "DevConfig/XCPMessageStruct.h"
#include <map>

typedef std::map<XCP_SlaveData, std::vector<XCP_DAQList> > XCP_MAP_Slave_DAQList;     // slave -- XCP_DAQList

int parse_xml_xcp_slave(rapidxml::xml_node<> *pXCPNode, const char *ChanName, int nNumSlaves);

#endif