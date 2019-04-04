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
    
    //zeromq������Ϣ�ص�
    static void ZeromqSubMsg(LPSUBMESSAGE pSubMessage, void * pUser);
    //����zmq����ͼƬ��Ϣ
    bool ParseZmqJson(LPSUBMESSAGE pSubMessage, int nImageAddress);
    //����ST���ص�����ֵ��Ϣ
    bool ParseSTResponJson(LPSUBMESSAGE pSubMessage, string sRespon);
    //�������ͼƬ����ֵ�������Ϣ, �ɷ����������
    bool SendImageInfo(LPSUBMESSAGE pSubMessage);

    //��ʱ��ȡ��ǰ�ɼ����������ͷ�߳�
#ifdef __WINDOWS__
    static DWORD WINAPI GetRelativeCameraThread(void * pParam);
#else
    static void * GetRelativeCameraThread(void * pParam);
#endif
    void GetRelativeCameraAction();

    //������Ϣ�����߳�
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
    //��������ͼƬΨһUUID
    virtual std::string GetUUID();
    unsigned long long ChangeTimeToSecond(string sTime);
private:
    int m_nServiceID;       //����ID
    string m_sServiceIP;    //����IP
    int m_nServicePort;     //����Port

    string m_sRedisIP;
    int m_nRedisPort;

    string m_sZmqIP;
    int m_nZmqPort;

    vector<LPSTSERVERINFO> m_vSTServerInfo;    //������������Ϣ

    set<string> m_setRelativeCamera;            //������ͷ����
    list<LPSUBMESSAGE> m_listPubMessage;        //������Ϣ����

    CConfigRead  m_ConfigRead;
    CMySQLOperation * m_pMysqlOperation;        //DB����
    CZeromqManage * m_pZeromqManage;            //Zeromq����
    CRedisManage * m_pRedisManage;              //Redis����

    bool m_bStopService;                        //����ֹͣ��־λ
    LISTSUBMESSAGE m_listSubMessageResource;    //��Դ��

    int m_nThreadCount;                         //�߳���, =�����ļ�ָ��*��������������, ��󲻳���128
#ifdef __WINDOWS__
    CRITICAL_SECTION m_cs;
    HANDLE m_hStopEvent;
    HANDLE m_hSubMsgEvent;
    HANDLE m_hThreadSubMsgCommand[MAXTHREADCOUNT];              //������Ϣ�����߳̾��
    HANDLE m_hThreadGetRelativeCamera;          //��ȡ��ǰ�ɼ�������ͷ�߳̾��
#else
    pthread_mutex_t m_mutex;                    //�ٽ���
    int m_nPipeStop[2];
    int m_nPipeSubMsg[2];
    pthread_t m_hThreadSubMsgCommand[MAXTHREADCOUNT];           //������Ϣ�����߳̾��
    pthread_t m_hThreadGetRelativeCamera;       //��ȡ��ǰ�ɼ�������ͷ�߳̾��
#endif

    int m_nTotalSnapCount;  //���յ���ץ������
    char m_pImageAddress[4][20];
};

