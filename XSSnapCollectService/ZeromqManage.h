#ifndef zeromqmanage_h

#include "DataDefine.h"
#include <list>
#include <zeromq/zmq.h>
#include "LogRecorder.h"
#ifdef _WINDOWS_
#pragma comment( lib, "libzmq-v100-mt-gd-4_0_4.lib" )
#else
#include <unistd.h>
#endif
using namespace std;

class CZeromqManage
{
public:
    CZeromqManage();
    ~CZeromqManage();
public:
    void UnInit();
    //������Ϣ
    bool InitPub(char * pLocalIP, int nLocalPort, char * pServerIP, int nServerPort);
    bool PubMessage(LPSUBMESSAGE pSubMessage);
    bool PubMessage(const char * pHead, char * pID);
    //������Ϣ
    bool InitSub(char * pLocalIP, int nLocalPort, char * pServerIP, int nServerPort,
                 LPSubMessageCallback pSubMessageCallback, void * pUser, int nThreadNum = THREADNUM);
    bool AddSubMessage(const char * pSubMessage);
    bool DelSubMessage(char * pSubMessage);

    //�󶨱���push�˿�
    bool InitPush(char * pLocalIP, int nLocalPushPort, char * pServerIP, int nServerPort);
    bool PushMessage(LPSUBMESSAGE pSubMessage);
    //pull
    bool InitPull(char * pLocalIP, int nLocalPushPort, char * pServerIP, int nServerPort, 
                    LPSubMessageCallback pSubMessageCallback, void * pUser, int nThreadNum = THREADNUM);
private:
    //������Ϣ����
#ifdef _WINDOWS_
    static DWORD WINAPI SubMessageThread(void * pParam);
    static DWORD WINAPI PullMessageThread(void * pParam);
    static DWORD WINAPI MessageCallbackThread(void * pParam);
#else
    static void * SubMessageThread(void * pParam);
    static void * PullMessageThread(void * pParam);
    static void * MessageCallbackThread(void * pParam);
#endif
    //sub��Ϣ����
    void SubMessageAction();
    //pull��Ϣ����
    void PullMessageAction();
    //sub, pull������Ϣ�ص�
    void MessageCallbackAction();

    LPSUBMESSAGE GetFreeResource();
    void FreeResource(LPSUBMESSAGE pSubMessage);
    //�����ٽ���
    void EnterMutex();
    //�˳��ٽ���
    void LeaveMutex();
    void MsgNotify();

    void EnterPubMutex();
    void LeavePubMutex();
private:
    LPSubMessageCallback m_pSubMsgCallback; //������Ϣ�ص�
    void * m_pUser;                         //������Ϣ�ص����س�ʼ���������
    bool m_bStopFlag;                       //�߳�ֹͣ��־
    int m_nThreadNum;                       //�ص��߳���, ������Դ*20

    void * m_pCtx;
    //����Pub
    void * m_pPubSocket;            //����socket
    //����Sub
    void * m_pSubSocket;            //����socket
    //����Push
    void * m_pPushSocket;            //����socket
    //��Pull
    void * m_pPullSocket;            //��socket

#ifdef _WINDOWS_
    CRITICAL_SECTION m_cs;
    CRITICAL_SECTION m_Pubcs;
    HANDLE m_ThreadSubMessageID;
    HANDLE m_ThreadPullMessageID;
    HANDLE m_ThreadCallbackID[THREADNUM];
    HANDLE m_hMsgEvent;
#else
    pthread_mutex_t m_mutex;                    //�ٽ���
    pthread_mutex_t m_PubMutex;                 //������Ϣ�ٽ���
    pthread_t m_ThreadSubMessageID;             //������Ϣ�߳̾��
    pthread_t m_ThreadPullMessageID;            //pull��Ϣ�߳̾��
    pthread_t m_ThreadCallbackID[THREADNUM];    //�ص��߳̾��
    int m_nPipe[2];
#endif
    LISTSUBMESSAGE m_listSubMessageResource;
    LISTSUBMESSAGE m_listSubMessageCallback;
};
#define zeromqmanage_h
#endif
