#pragma once
#include "stdafx.h"
#include <string>
#include <list>
#include <map>
#include <vector>
#include "LogRecorder.h"
#include "ZeromqManage.h"
#include "ConfigRead.h"
#include "MySQLOperation.h"
#include "RedisManage.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "hpsocket/hpsocket.h"
#include <objbase.h>

using namespace std;

#define IMAGEADDRESS1   "44.53.5.51"
#define IMAGEADDRESS2   "44.53.5.52"
#define IMAGEADDRESS3   "44.53.5.53"
#define IMAGEADDRESS4   "44.53.4.100"


typedef struct _STServerInfo
{
    char pIP[64];
    int nPort;
    _STServerInfo()
    {
        memset(pIP, 0, 64);
        nPort = 0;
    }
}STSERVERINFO, *LPSTSERVERINFO;

class CSnapCollectService
{
public:
    CSnapCollectService();
    ~CSnapCollectService();
public:
    bool StartService();
    bool StopService();
private:
    bool Init();
    bool InitDB();
    bool InitZeromq();
    bool InitRedis();
    
    //zeromq订阅消息回调
    static void ZeromqSubMsg(LPSUBMESSAGE pSubMessage, void * pUser);
    //解析zmq订阅图片信息
    bool ParseZmqJson(LPSUBMESSAGE pSubMessage, int nImageAddress);
    //解析ST返回的特征值信息
    bool ParseSTResponJson(LPSUBMESSAGE pSubMessage, string sRespon);
    //向代理发送图片特征值及相关信息, 由分析服务接收
    bool SendImageInfo(LPSUBMESSAGE pSubMessage);

    //定时获取当前采集服务关联镜头线程
#ifdef __WINDOWS__
    static DWORD WINAPI GetRelativeCameraThread(void * pParam);
#else
    static void * GetRelativeCameraThread(void * pParam);
#endif
    void GetRelativeCameraAction();

    //订阅消息处理线程
#ifdef __WINDOWS__
    static DWORD WINAPI SubMessageCommandThread(void * pParam);
#else
    static void * SubMessageCommandThread(void * pParam);
#endif
    void SubMessageCommandAction();

    LPSUBMESSAGE GetFreeResource();
    void FreeResource(LPSUBMESSAGE pSubMessage);
    void EnterMutex();
    void LeaveMutex();
    //生成人脸图片唯一UUID
    virtual std::string GetUUID();
    unsigned long long ChangeTimeToSecond(string sTime);
private:
    int m_nServiceID;       //服务ID
    string m_sServiceIP;    //服务IP
    int m_nServicePort;     //服务Port

    string m_sRedisIP;
    int m_nRedisPort;

    string m_sZmqIP;
    int m_nZmqPort;

    vector<LPSTSERVERINFO> m_vSTServerInfo;    //商汤服务器信息

    set<string> m_setRelativeCamera;            //关联镜头集合
    list<LPSUBMESSAGE> m_listPubMessage;        //订阅消息链表

    CConfigRead  m_ConfigRead;
    CMySQLOperation * m_pMysqlOperation;        //DB连接
    CZeromqManage * m_pZeromqManage;            //Zeromq管理
    CRedisManage * m_pRedisManage;              //Redis管理

    bool m_bStopService;                        //服务停止标志位
    LISTSUBMESSAGE m_listSubMessageResource;    //资源池

    int m_nThreadCount;                         //线程数, =配置文件指定*商汤服务器个数, 最大不超过128
#ifdef __WINDOWS__
    CRITICAL_SECTION m_cs;
    HANDLE m_hStopEvent;
    HANDLE m_hSubMsgEvent;
    HANDLE m_hThreadSubMsgCommand[MAXTHREADCOUNT];              //订阅消息处理线程句柄
    HANDLE m_hThreadGetRelativeCamera;          //获取当前采集关联镜头线程句柄
#else
    pthread_mutex_t m_mutex;                    //临界区
    int m_nPipeStop[2];
    int m_nPipeSubMsg[2];
    pthread_t m_hThreadSubMsgCommand[MAXTHREADCOUNT];           //订阅消息处理线程句柄
    pthread_t m_hThreadGetRelativeCamera;       //获取当前采集关联镜头线程句柄
#endif

    int m_nTotalSnapCount;  //接收的总抓拍数量
    char m_pImageAddress[4][20];
};

