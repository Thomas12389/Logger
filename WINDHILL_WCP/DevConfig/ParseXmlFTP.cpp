#include "DevConfig/ParseXmlFTP.hpp"

#include <iostream>
#include <cstring>

int parse_xml_ftp(rapidxml::xml_node<> *pFTPNode, FTP_Server& FTP_Ser){

    rapidxml::xml_node<> *pNode = pFTPNode->first_node("active");
    if (NULL == pNode) {
        return -1;
    }
    char *pActive = pNode->value();
    if (0 == strncmp("true", pActive, strlen("true"))) {
        FTP_Ser.bIsActive = true;
    } else {
        FTP_Ser.bIsActive = false;
        return 0;
    }

    pNode = pFTPNode->first_node("IP");
    if (NULL == pNode) {
        return -1;
    }
    FTP_Ser.strIP = pNode->value();

    pNode = pFTPNode->first_node("Port");
    if (NULL == pNode) {
        return -1;
    }
    FTP_Ser.nPort = atoi(pNode->value());

    pNode = pFTPNode->first_node("Username");
    if (NULL == pNode) {
        return -1;
    }
    FTP_Ser.strUserName = pNode->value();

    pNode = pFTPNode->first_node("Passwd");
    if (NULL == pNode) {
        return -1;
    }
    FTP_Ser.strPasswd = pNode->value();

    pNode = pFTPNode->first_node("securedMode");
    if (NULL == pNode) {
        return -1;
    }
    char *pSFTPActive = pNode->value();
    if (0 == strncmp("true", pSFTPActive, strlen("true"))) {
        FTP_Ser.bIsSFTP = true;
    } else {
        FTP_Ser.bIsSFTP = false;
    }

    pNode = pFTPNode->first_node("deleteAfterUpload");
    if (NULL == pNode) {
        FTP_Ser.bDeleteAfterUpload = false;
        // return -1;
        return 0;
    }
    char *pDeleteAfterUpload = pNode->value();
    if (0 == strncmp("true", pDeleteAfterUpload, strlen("true"))) {
        FTP_Ser.bDeleteAfterUpload = true;
    } else {
        FTP_Ser.bDeleteAfterUpload = false;
    }

    return 0;
}