
#ifndef __PARSEXMLINSIDE_H__
#define __PARSEXMLINSIDE_H__

#include <map>

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "DevConfig/InsideMessageStruct.h"

int parse_xml_inside(rapidxml::xml_node<> *pGPSNode, const char *pSource);

#endif