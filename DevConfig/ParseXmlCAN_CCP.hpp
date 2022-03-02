#ifndef __PARSEXMLCAN_CCP_H__
#define __PARSEXMLCAN_CCP_H__

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "DevConfig/CCPMessageStruct.h"
#include <map>

typedef std::map<CCP_SlaveData, std::vector<CCP_DAQList> > CCP_MAP_Slave_DAQList;     // slave -- CCP_DAQList

int parse_xml_ccp_slave(rapidxml::xml_node<> *pCCPNode, const char *ChanName, int nNumSlaves);
int parse_xml_ccp_daq(CCP_SlaveData SlaveData, rapidxml::xml_node<> *pDaqListsNode, const char *ChanName);
int parse_xml_ccp_odt(CCP_SlaveData SlaveData, rapidxml::xml_node<> *pDaqListNode, const char *ChanName);
int parse_xml_ccp_element(rapidxml::xml_node<> *pODTNode, CCP_ODT *pCCP_ODT);


#endif