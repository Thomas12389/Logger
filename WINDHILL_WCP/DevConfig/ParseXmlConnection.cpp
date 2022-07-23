
#include <iostream>
#include <cstring>

#include "tbox/Common.h"
#include "DevConfig/ParseXmlConnection.hpp"

extern Connection g_Connection;

int parse_xml_LAN(rapidxml::xml_node<> *pConnectionLANNode) {

    return 0;
}

int parse_xml_Modem(rapidxml::xml_node<> *pConnectionModemNode) {

    char *pActive = pConnectionModemNode->first_node("active")->value();

    if (0 == strncmp("true", pActive, strlen("true"))) {
        g_Connection.Modem_Conn.bActive = true;
        g_Connection.Modem_Conn.userName = pConnectionModemNode->first_node("userName")->value();
        g_Connection.Modem_Conn.passwd = pConnectionModemNode->first_node("passwd")->value();
        g_Connection.Modem_Conn.APN = pConnectionModemNode->first_node("acessPonitName")->value();
    } else {
        g_Connection.Modem_Conn.bActive = false;
    }

    return 0;
}

int parse_xml_WLAN(rapidxml::xml_node<> *pConnectionWLANNode) {
    int numWLAN = atoi(pConnectionWLANNode->first_attribute("num")->value());
    numWLAN = 1;      // 目前只支持一个

    if (!numWLAN) return -1;

    rapidxml::xml_node<> *pWLANNode = pConnectionWLANNode->first_node("WLAN");

    g_Connection.pWLAN_CONN = new WLAN_CONN[numWLAN];
    g_Connection.nWLAN = 0;

    while (pWLANNode && numWLAN--) {

        char *pActive = pWLANNode->first_node("active")->value();
        if (0 == strncmp("true", pActive, strlen("true"))) {

            g_Connection.pWLAN_CONN->SSID = pWLANNode->first_node("SSID")->value();
            g_Connection.pWLAN_CONN->passwd = pWLANNode->first_node("passwd")->value();

            g_Connection.nWLAN++;
        }

        pWLANNode = pWLANNode->next_sibling("WLAN");
    }

    if (g_Connection.nWLAN) {
        // 执行脚本生成 WLAN 配置文件
        // system("/opt/scripts/WLANstart.sh");
    } else {
        delete[] g_Connection.pWLAN_CONN;
    }

    return 0;
}


int parse_xml_connection(rapidxml::xml_node<> *pConnectionNode){

    int retVal = 0;
    // 解析 WLAN
    rapidxml::xml_node<> *pConnectionSubode = pConnectionNode->first_node("ConnectionWLAN");
    if (NULL == pConnectionSubode) {
        return -1;
    }
    retVal = parse_xml_WLAN(pConnectionSubode);
    if (-1 == retVal) return -1;

    // 解析 LAN
    pConnectionSubode = pConnectionNode->first_node("ConnectionLAN");
    if (NULL == pConnectionSubode) {
        return -1;
    }
    retVal = parse_xml_LAN(pConnectionSubode);
    if (-1 == retVal) return -1;

    // 解析 Modem
    pConnectionSubode = pConnectionNode->first_node("ConnectionModem");
    if (NULL == pConnectionSubode) {
        return -1;
    }
    retVal = parse_xml_Modem(pConnectionSubode);
    if (-1 == retVal) return -1;

    return 0;
}