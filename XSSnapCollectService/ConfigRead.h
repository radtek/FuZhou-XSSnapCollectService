#ifndef configread_h

#include <iostream>  
#include <string>  
#include <fstream> 
#include <stdio.h>
#include <stdlib.h>
using namespace std;

class CConfigRead
{
public:
    CConfigRead(void);
    ~CConfigRead(void);
public:
#ifdef __WINDOWS__
    string GetCurrentPath();
#endif
    bool ReadConfig();
public:
    string m_sConfigFile;
    string m_sCurrentPath;

    string m_sServerID;     //服务ID
    string m_sDBIP;         //DB IP
    int m_nDBPort;          //DB Port
    string m_sDBName;       //DB Name
    string m_sDBUser;       //DB User
    string m_sDBPd;         //DB Password

    int m_nThreadCount;     //线程数
};

#define configread_h
#endif