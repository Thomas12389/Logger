#include "DevConfig/ParseXmlFTP.hpp"

#include <iostream>

int parse_xml_ftp(rapidxml::xml_node<> *pFTPNode, FTP_Server& FTP_Ser){

    rapidxml::xml_node<> *pNode = pFTPNode->first_node("IP");
    FTP_Ser.strIP = pNode->value();

    pNode = pFTPNode->first_node("Port");
    FTP_Ser.nPort = atoi(pNode->value());

    pNode = pFTPNode->first_node("Username");
    FTP_Ser.strUserName = pNode->value();

    pNode = pFTPNode->first_node("Passwd");
    FTP_Ser.strPasswd = pNode->value();

    return 0;
}