#ifndef __PARSE_XML_STORAGE_H_
#define __PARSE_XML_STORAGE_H_

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp" 
#include "DevConfig/PhyChannel.hpp"

int parse_xml_storage(rapidxml::xml_node<> *pStorageNode, File_Save& File_Info);

#endif