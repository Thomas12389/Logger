#ifndef __PARSE_XML_MQTT_H_
#define __PARSE_XML_MQTT_H_

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp" 
#include "DevConfig/PhyChannel.hpp"

int parse_xml_mqtt(rapidxml::xml_node<> *pMQTTNode, MQTT_Server& MQTT_Ser);

#endif