#ifndef MySQLOperation_h

#include <mysql/mysql.h>
#include "DataDefine.h"
#ifdef __WINDOWS__
#pragma comment( lib, "libmysql.lib" )
#else
#include <unistd.h>
#include <stdio.h>
#endif
class CMySQLOperation
{
public:
    CMySQLOperation();
    ~CMySQLOperation();
public:
    bool ConnectDB(const char * pIP, const char * pUser, const char * pPassword, const char * pDBName, int nPort);
    bool DisConnectDB();
    //ִ��SQL���
    bool QueryCommand(char * pSQL);
    //ȡ����ѯ�����
    bool GetQueryResult(char * pSQL, MYSQL_RES * &pResult);

    void EnterMutex();
    void LeaveMutex();
private:
    MYSQL m_mysql;     //����MySQL���ݿ�
#ifdef __WINDOWS__
    CRITICAL_SECTION m_cs;
#else
    pthread_mutex_t m_mutex;            //�ٽ���
#endif
};

#define MySQLOperation_h
#endif