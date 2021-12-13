
#include "tbox/Common.h"

#include "DevConfig/ParseConfiguration.h"
#include "DevConfig/ParseXmlCAN.hpp"
#include "DevConfig/ParseXmlMQTT.hpp"
#include "DevConfig/ParseXmlFTP.hpp"
#include "DevConfig/ParseXmlSTORAGE.hpp"
#include "DevConfig/ParseXmlInside.hpp"

#include "Logger/Logger.h"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <unistd.h>
#include <string>

// CAN 通道信号
Channel_MAP g_Channel_MAP;
// 设备内置信号（包括内置 GPS）
Inside_MAP g_Inside_MAP;
// 服务器相关信息
Net_Info g_Net_Info;
// 设备自身相关信息
Hardware_Info g_Hardware_Info;
// 文件保存相关信息
File_Save g_File_Info;

int parse_configuration(const char *pPath, const std::string strDate) {
    if (-1 == access(pPath, F_OK)) {
        XLOG_ERROR("The configuration file doesn't exist!");
        return -1;
    }
    // read xml
    rapidxml::file<> fdoc(pPath);

    rapidxml::xml_document<> doc;  // character type defaults to char
    doc.parse<0>(fdoc.data());     // 0 means default parse flags

    // Get Xml Root Node
    rapidxml::xml_node<> *pRoot = doc.first_node("CONFIGURATION");
    if (!pRoot) {
        XLOG_ERROR("Incorrect configuration file format!(ROOT)");
        return -1;
    }

    // 获取相关全局信息
    rapidxml::xml_node<> *pNode = pRoot->first_node("FlagsModel");
    // rapidxml::xml_node<> *pSubNode = pNode->first_node("Serial_Number");
    // g_Hardware_Info.SeqNum = pSubNode->value();
    g_Hardware_Info.SeqNum = "Logger_";
    uint8_t serial_num[7] = {0};
    GetSerialNumber(serial_num);
    std::string str_serial((char *)serial_num);
    g_Hardware_Info.SeqNum += str_serial;

    // 解析 GPS 信号
    pNode = pRoot->first_node("GPS");
    parse_xml_inside(pNode, pNode->name());
    // 解析内置信号
    pNode = pRoot->first_node("Internal");
    parse_xml_inside(pNode, pNode->name());

    // 解析 CAN 相关参数
    pNode = pRoot->first_node("CAN");
    if (NULL == pNode) {
        XLOG_ERROR("Incorrect configuration file format(CAN)!");
        return -1;
    }

    if (-1 == parse_xml_can(pNode)) {
        XLOG_ERROR("Parse CAN configuration ERROR!");
        return -1;
    }

    // 解析 MQTT 服务器参数及所需的输出信号
    pNode = pRoot->first_node("MQTT");
    if (NULL == pNode) {
        XLOG_ERROR("Incorrect configuration file format(MQTT)!");
    }
    if (-1 == parse_xml_mqtt(pNode, g_Net_Info.mqtt_server)) {
        XLOG_ERROR("Parse MQTT configuration ERROR!");
        return -1;
    }

    // 解析 FTP 服务器参数
    pNode = pRoot->first_node("FTP");
    if (NULL == pNode) {
        XLOG_ERROR("Incorrect configuration file format(FTP)!");
    }
    if (-1 == parse_xml_ftp(pNode, g_Net_Info.ftp_server)) {
        XLOG_ERROR("Parse FTP configuration ERROR!");
        return -1;
    }

    // 解析需要保存到文件的参数
    pNode = pRoot->first_node("STORAGE");
    if (NULL == pNode) {
        XLOG_ERROR("Incorrect configuration file format(STOREAGE)!");
    }
    g_File_Info.strRemoteDirName = "/" + g_Hardware_Info.SeqNum;
    g_File_Info.strLocalDirName = strDate;
    if (-1 == parse_xml_storage(pNode, g_File_Info)) {
        XLOG_ERROR("Parse STORAGE configuration ERROR!");
        return -1;
    }

    return 0;
}