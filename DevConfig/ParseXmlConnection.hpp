#ifndef __PARSE_XML_CONNECTION_H_
#define __PARSE_XML_CONNECTION_H_

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp" 

// #include "DevConfig/PhyChannel.hpp"
#include <inttypes.h>

int parse_xml_connection(rapidxml::xml_node<> *pConnectionNode);

#endif