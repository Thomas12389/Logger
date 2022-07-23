
#ifndef __PARSEXMLCAN_H__
#define __PARSEXMLCAN_H__

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <map>

struct CAN_BuardRate {
    uint32_t nCANBR;
    uint32_t nCANFDBR;
};


int parse_xml_can(rapidxml::xml_node<> *pCANNode);

#endif