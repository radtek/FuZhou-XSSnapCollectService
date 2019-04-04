#include "stdafx.h"
#include "RedisManage.h"

#ifdef __WINDOWS__
extern CLogRecorder g_LogRecorder;
CRedisManage::CRedisManage()
{
    InitializeCriticalSection(&m_cs);
}
CRedisManage::~CRedisManage()
{
    DeleteCriticalSection(&m_cs);
}
bool CRedisManage::InitRedis(const char * pRedisIP, int nRedisPort)
{
    sprintf_s(m_pRedisAddr, "%s:%d", pRedisIP, nRedisPort);
    printf("Redis address: %s\n", m_pRedisAddr);
    acl::acl_cpp_init();
    acl::log::stdout_open(true);
    m_pClient = new acl::redis_client(m_pRedisAddr, 20, 10);
    m_pRedisCmd = new acl::redis(m_pClient);
    return true;
}
void CRedisManage::UnInit()
{
    delete m_pRedisCmd;
    delete m_pClient;
    return;
}
bool CRedisManage::ZAddSortedSet(const char * pKey, double nScore[], const char *pMember[], int nSize)
{
    bool bRet = false;
    EnterCriticalSection(&m_cs);
    int nRet = m_pRedisCmd->zadd(pKey, pMember, nScore, nSize);
    if (-1 == nRet)
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: zadd key[%s] Failed. ", pKey);
        g_LogRecorder.WriteErrorLog(__FUNCTION__, "****Error: ����� key ��������򼯶���");
        bRet = false;
    }
    else if (0 == nRet)
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: zadd key[%s] Failed. ", pKey);
        g_LogRecorder.WriteErrorLog(__FUNCTION__, "****Error: һ��Ҳδ��ӣ�������Ϊ�ó�Ա�Ѿ�������������");
        bRet = false;
    }
    else if (0 < nRet)
    {
        //g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "zadd key[%s] success. ", pKey);
        SetExpireTime(pKey, HASHOVERTIME);
        bRet = true;
    }
    LeaveCriticalSection(&m_cs);

    return bRet;
}
bool CRedisManage::InsertHashValue(const char * pKey, const char * pName, const char * pValue, int nValueLen)
{
    EnterCriticalSection(&m_cs);
    if (m_pRedisCmd->hset(pKey, pName, pValue, nValueLen) < 0)
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__,
            "****Error: hset Name[%s] To key[%s] Failed", pName, pKey);
        LeaveCriticalSection(&m_cs);
        return false;
    }
    SetExpireTime(pKey, HASHOVERTIME);
    LeaveCriticalSection(&m_cs);
    return true;
}
bool CRedisManage::InsertHashValue(const char * pKey, std::map<acl::string, acl::string> mapValues)
{
    EnterCriticalSection(&m_cs);
    if (m_pRedisCmd->hmset(pKey, mapValues) < 0)
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__,
            "****Error: hset Values To key[%s] Failed!", pKey);
        LeaveCriticalSection(&m_cs);
        return false;
    }
    SetExpireTime(pKey, HASHOVERTIME);
    LeaveCriticalSection(&m_cs);
    return true;
}
void CRedisManage::SetExpireTime(const char * pKey, int nSecond)
{
    m_pRedisCmd->expire(pKey, nSecond);
}
#else
CRedisManage::CRedisManage()
{
    m_pRedisContext = NULL;
    m_nRedisPort = 0;
    pthread_mutex_init(&m_mutex, NULL);
}
CRedisManage::~CRedisManage()
{
    pthread_mutex_destroy(&m_mutex);
}
bool CRedisManage::InitRedis(const char * pRedisIP, int nRedisPort)
{
    strcpy(m_pRedisIP, pRedisIP);
    m_nRedisPort = nRedisPort;

    printf("Redis address: %s:%d\n", m_pRedisIP, m_nRedisPort);
    struct timeval timeout = { 2, 0 };
    m_pRedisContext = (redisContext*)redisConnectWithTimeout(pRedisIP, nRedisPort, timeout);
    if ((NULL == m_pRedisContext) || (m_pRedisContext->err))
    {
        if (m_pRedisContext)
        {
            printf("connect error: %s.\n", m_pRedisContext->errstr);
        }
        else
        {
            printf("connect error: can't allocate redis context.\n");
        }
        return false;
    }
    else
    {
        printf("connect Redis[%s:%d] success!\n", pRedisIP, nRedisPort);
    }
    return true;
}
void CRedisManage::UnInit()
{
    if (NULL != m_pRedisContext)
    {
        redisFree(m_pRedisContext);
        m_pRedisContext = NULL;
    }
    return;
}
bool CRedisManage::ReconnectRedis()
{
    if (NULL != m_pRedisContext)
    {
        redisFree(m_pRedisContext);
        m_pRedisContext = NULL;
    }
    if (0 == m_nRedisPort)
    {
        return false;
    }

    struct timeval timeout = { 2, 0 };
    m_pRedisContext = (redisContext*)redisConnectWithTimeout(m_pRedisIP, m_nRedisPort, timeout);
    if ((NULL == m_pRedisContext) || (m_pRedisContext->err))
    {
        if (m_pRedisContext)
        {
            printf("connect error: %s.\n", m_pRedisContext->errstr);
        }
        else
        {
            printf("connect error: can't allocate redis context.\n");
        }
        return false;
    }
    return true;
}
bool CRedisManage::ZAddSortedSet(const char * pKey, double nScore[], const char *pMember[], int nSize)
{
    string sCommand = "zadd ";
    sCommand += pKey;
    char pCommand[1024] = { 0 };
    for (int i = 0; i < nSize; i++)
    {
        sprintf(pCommand, " %lf %s", nScore[i], pMember[i]);
        sCommand += pCommand;
    }
    if (!ExecuteCommand(sCommand.c_str()))
    {
        return false;
    }
    SetExpireTime(pKey, HASHOVERTIME);
    return true;
}
bool CRedisManage::InsertHashValue(const char * pKey, const char * pName, const char * pValue, int nValueLen)
{
    char pCommand[1024] = { 0 };
    sprintf(pCommand, "hset %s %s %s", pKey, pName, pValue);
    if (!ExecuteCommand(pCommand))
    {
        return false;
    }
    SetExpireTime(pKey, HASHOVERTIME);
    return true;
}
bool CRedisManage::InsertHashValue(const char * pKey, map<string, string> mapValues)
{
    string sCommand = "hmset ";
    sCommand += pKey;
    char pCommand[1024] = { 0 };
    for (map<string, string>::iterator it = mapValues.begin(); it != mapValues.end(); it++)
    {
        sprintf(pCommand, " %s %s", it->first.c_str(), it->second.c_str());
        sCommand += pCommand;
    }
    if (!ExecuteCommand(sCommand.c_str()))
    {
        return false;
    }
    SetExpireTime(pKey, HASHOVERTIME);
    return true;
}
void CRedisManage::SetExpireTime(const char * pKey, int nSecond)
{
    char pCommand[1024] = { 0 };
    sprintf(pCommand, "expire %s %d", pKey, nSecond);
    ExecuteCommand(pCommand);
    return;
}
bool CRedisManage::ExecuteCommand(const char * pCommand)
{
    bool bRet = false;

    if (NULL == m_pRedisContext)
    {
        if (!ReconnectRedis())
        {
            return false;
        }
    }

    pthread_mutex_lock(&m_mutex);
    redisReply * pRedisReply = (redisReply*)redisCommand(m_pRedisContext, pCommand);
    if (NULL == pRedisReply)    
    {
        printf("redisCommand return NULL.\n");
        if (!ReconnectRedis())
        {
            printf("Reconnect Redis[%s:%d] Failed.\n", m_pRedisIP, m_nRedisPort);
        }
        else
        {
            pRedisReply = (redisReply*)redisCommand(m_pRedisContext, pCommand);
        }
    }
    if (NULL != pRedisReply)
    {
        switch (pRedisReply->type)
        {
        case REDIS_REPLY_INTEGER:
            bRet = true;
            break;
        case REDIS_REPLY_STATUS:
            if (string(pRedisReply->str, pRedisReply->len) == "OK")
            {
                bRet = true;
            }
            break;
        case REDIS_REPLY_ERROR:
            printf("Execute Comman Failed: \n%s\n", string(pRedisReply->str, pRedisReply->len).c_str());
            printf("Command: %s.\n", pCommand);
            break;
        default:
            break;
        }
        freeReplyObject(pRedisReply);
    }
    pthread_mutex_unlock(&m_mutex);

    return bRet;
}
#endif