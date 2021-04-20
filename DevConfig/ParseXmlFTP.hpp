#ifndef __PARSE_XML_FTP_H_
#define __PARSE_XML_FTP_H_

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp" 

#include "DevConfig/PhyChannel.hpp"
#include <inttypes.h>

int parse_xml_ftp(rapidxml::xml_node<> *pFTPNode, FTP_Server& FTP_Ser);

#endif