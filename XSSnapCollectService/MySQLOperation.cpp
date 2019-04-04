#include "stdafx.h"
#include "MySQLOperation.h"


CMySQLOperation::CMySQLOperation()
{
    mysql_library_init(0, 0, 0);
#ifdef __WINDOWS__
    InitializeCriticalSection(&m_cs);
#else
    pthread_mutex_init(&m_mutex, NULL);
#endif
}


CMySQLOperation::~CMySQLOperation()
{
#ifdef __WINDOWS__
    DeleteCriticalSection(&m_cs);
#else
    pthread_mutex_destroy(&m_mutex);
#endif
}
bool CMySQLOperation::ConnectDB(const char * pIP, const char * pUser, const char * pPassword, const char * pDBName, int nPort)
{
    mysql_init(&m_mysql);
    char value = 1;
    mysql_options(&m_mysql, MYSQL_OPT_RECONNECT, (char *)&value);
    if (!mysql_real_connect(&m_mysql, pIP, pUser, pPassword, pDBName, nPort, NULL, 0))
    {
        const char * pErrorMsg = mysql_error(&m_mysql);
        printf("%s\n", pErrorMsg);
        printf("***Warning: CAnalyseServer::mysql_real_connect Failed, Please Check MySQL Service is start!\n");
        printf("DBInfo: %s:%s:%s:%s:%d\n", pIP, pUser, pPassword, pDBName, nPort);
        return false;
    }
    else
    {
        printf("CAnalyseServer::Connect MySQL Success!\n");
    }
    return true;
}
bool CMySQLOperation::DisConnectDB()
{
    mysql_close(&m_mysql);
    return true;
}
bool CMySQLOperation::QueryCommand(char * pSQL)
{
    bool bRet = false;
    EnterMutex();
    mysql_ping(&m_mysql);
    if (1 == mysql_query(&m_mysql, pSQL))
    {
        const char * pErrorMsg = mysql_error(&m_mysql);
        if (string(pErrorMsg).find("already exists") == string::npos)    //表己存在则不返回错误
        {
            printf("****Error: Excu SQL Failed, SQL:\n%s\nError: %s\n", pSQL, pErrorMsg);
        }
        else
        {
            bRet = true;
        }
    }
    else
    {
        bRet = true;
    }
    LeaveMutex();
    return bRet;
}
bool CMySQLOperation::GetQueryResult(char * pSQL, MYSQL_RES * &pResult)
{
    bool bRet = false;
    EnterMutex();
    mysql_ping(&m_mysql);
    if (1 == mysql_query(&m_mysql, pSQL))
    {
        const char * pErrorMsg = mysql_error(&m_mysql);
        printf("****Error: Excu SQL Failed, SQL:\n%s\nError: %s\n", pSQL, pErrorMsg);
    }
    else
    {
        pResult = mysql_store_result(&m_mysql);
        if (NULL == pResult)
        {
            const char * pErrorMsg = mysql_error(&m_mysql);
            printf("****Error: mysql_store_result Failed, SQL:\n%s\n", pSQL);
        }
        else
        {
            bRet = true;
        }
    }
    LeaveMutex();
    return bRet;
}
void CMySQLOperation::EnterMutex()
{
#ifdef __WINDOWS__
    EnterCriticalSection(&m_cs);
#else
    pthread_mutex_lock(&m_mutex);
#endif
}
void CMySQLOperation::LeaveMutex()
{
#ifdef __WINDOWS__
    LeaveCriticalSection(&m_cs);
#else
    pthread_mutex_unlock(&m_mutex);
#endif
}