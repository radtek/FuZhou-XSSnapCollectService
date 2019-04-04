#include "stdafx.h"
#include "ZeromqManage.h"


extern CLogRecorder g_LogRecorder;

CZeromqManage::CZeromqManage()
{
    m_pSubMsgCallback = NULL;
    m_pUser = NULL;
    m_bStopFlag = false;

    m_pCtx = NULL;
    m_pPubSocket = NULL;
    m_pSubSocket = NULL;
    m_pPullSocket = NULL;
    m_pPushSocket = NULL;
#ifdef _WINDOWS_
    m_ThreadSubMessageID = INVALID_HANDLE_VALUE;;
    m_ThreadPullMessageID = INVALID_HANDLE_VALUE;
    for (int i = 0; i < THREADNUM; i++)
    {
        m_ThreadCallbackID[i] = INVALID_HANDLE_VALUE;
    }
    InitializeCriticalSection(&m_cs);
    InitializeCriticalSection(&m_Pubcs);
    m_hMsgEvent = CreateEvent(NULL, true, false, NULL);
#else
    m_ThreadSubMessageID = -1;
    m_ThreadPullMessageID = -1;
    for (int i = 0; i < THREADNUM; i++)
    {
        m_ThreadCallbackID[i] = -1;
    }
    pthread_mutex_init(&m_mutex, NULL);
    pthread_mutex_init(&m_PubMutex, NULL);
    pipe(m_nPipe);
#endif
}

CZeromqManage::~CZeromqManage()
{
#ifdef _WINDOWS_
    DeleteCriticalSection(&m_cs);
    DeleteCriticalSection(&m_Pubcs);
    CloseHandle(m_hMsgEvent);
#else
    pthread_mutex_destroy(&m_mutex);
    pthread_mutex_destroy(&m_PubMutex);
    close(m_nPipe[0]);
    close(m_nPipe[1]);
#endif
}
bool CZeromqManage::InitSub(char * pLocalIP, int nLocalPort, char * pServerIP, int nServerPort,
                            LPSubMessageCallback pSubMessageCallback, void * pUser, int nThreadNum)
{
    m_pSubMsgCallback = pSubMessageCallback;
    m_pUser = pUser;
    m_nThreadNum = nThreadNum;

    if (NULL == m_pCtx)
    {
        if (NULL == (m_pCtx = zmq_ctx_new()))
        {
            printf("****Error: zmq_ctx_new Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    
    if (NULL == m_pSubSocket)
    {
        if (NULL == (m_pSubSocket = zmq_socket(m_pCtx, ZMQ_SUB)))
        {
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_socket Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    if (NULL != pLocalIP && 0 != nLocalPort)
    {
        char pAddress[32] = { 0 };
        sprintf(pAddress, "tcp://%s:%d", pLocalIP, nLocalPort);
        if (zmq_bind(m_pSubSocket, pAddress) < 0)
        {
            zmq_close(m_pSubSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_bind[%s:%d] Failed[%s]!\n", pLocalIP, nLocalPort, zmq_strerror(errno));
            return false;
        }
    }
    if (NULL != pServerIP && 0 != nServerPort)
    {
        char pSubAddress[32] = { 0 };
        sprintf(pSubAddress, "tcp://%s:%d", pServerIP, nServerPort);
        if (zmq_connect(m_pSubSocket, pSubAddress) < 0)
        {
            zmq_close(m_pSubSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_connect Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
        printf("sub zeromq[%s] success.\n", pSubAddress);
    }
    
    for (int i = 0; i < m_nThreadNum * 20; i++)
    {
        LPSUBMESSAGE pSubMessage = new SUBMESSAGE;
        m_listSubMessageResource.push_back(pSubMessage);
    }

#ifdef _WINDOWS_
    if (INVALID_HANDLE_VALUE == m_ThreadSubMessageID)
    {
        m_ThreadSubMessageID = CreateThread(NULL, 0, SubMessageThread, this, NULL, 0);   //�����߳�
    }
    for (int i = 0; i < m_nThreadNum; i++)
    {
        if (INVALID_HANDLE_VALUE == m_ThreadCallbackID[i])
        {
            m_ThreadCallbackID[i] = CreateThread(NULL, 0, MessageCallbackThread, this, NULL, 0);   //������Ϣ�ص��߳�
        }
    }
#else
    if (-1 == m_ThreadSubMessageID)
    {
        pthread_create(&m_ThreadSubMessageID, NULL, SubMessageThread, (void*)this);
    }
    for (int i = 0; i < m_nThreadNum; i++)
    {
        if (-1 == m_ThreadCallbackID[i])
        {
            pthread_create(&m_ThreadCallbackID[i], NULL, MessageCallbackThread, (void*)this);
        }
    }
#endif
    return true;
}
void CZeromqManage::UnInit()
{
    m_bStopFlag = true;
    MsgNotify();
#ifdef _WINDOWS_
    Sleep(10);
#else
    usleep(10 * 1000);
#endif

    //�ر� pub socket
    if (NULL != m_pPubSocket)
    {
        zmq_close(m_pPubSocket);
        m_pPubSocket = NULL;
    }
    //�ر� push socket
    if (NULL != m_pPushSocket)
    {
        zmq_close(m_pPushSocket);
        m_pPushSocket = NULL;
    }
    
    //�ر�context(��ֱ�ӹر�sub socket �� pull socket, �ᵼ������������zmq_recv����),
    //zmq_ctx_destroy ��һֱ����, ֱ�����������ڴ˵�socketȫ���ر�
    if (NULL != m_pCtx)
    {
        zmq_ctx_destroy(m_pCtx);
        m_pCtx = NULL;
    }
    //sub socket, pull socket���߳��йر�, �����ڴ˴���, �ر�contextʱ, zmq_recv����-1
#ifdef _WINDOWS_
    if (INVALID_HANDLE_VALUE != m_ThreadSubMessageID)
    {
        WaitForSingleObject(m_ThreadSubMessageID, INFINITE);
        m_ThreadSubMessageID = INVALID_HANDLE_VALUE;
        printf("--Thread Sub Message End...\n");
    }
    if (INVALID_HANDLE_VALUE != m_ThreadPullMessageID)
    {
        WaitForSingleObject(m_ThreadPullMessageID, INFINITE);
        m_ThreadPullMessageID = INVALID_HANDLE_VALUE;
        printf("--Thread Pull Message End...\n");
    }
    for (int i = 0; i < m_nThreadNum; i++)
    {
        if (INVALID_HANDLE_VALUE != m_ThreadCallbackID[i])
        {
            WaitForSingleObject(m_ThreadCallbackID[i], INFINITE);
            m_ThreadCallbackID[i] = INVALID_HANDLE_VALUE;
            printf("--Thread[%d] Callback Sub Message End...\n", i);
        }
    }
#else
    if (-1 != m_ThreadSubMessageID)
    {
        pthread_join(m_ThreadSubMessageID, NULL);
        m_ThreadSubMessageID = -1;
        printf("--Thread Sub Message End...\n");
    }
    if (-1 != m_ThreadPullMessageID)
    {
        pthread_join(m_ThreadPullMessageID, NULL);
        m_ThreadPullMessageID = -1;
        printf("--Thread Pull Message End...\n");
    }
    for (int i = 0; i < m_nThreadNum; i++)
    {
        if (-1 != m_ThreadCallbackID[i])
        {
            pthread_join(m_ThreadCallbackID[i], NULL);
            m_ThreadCallbackID[i] = -1;
            printf("--Thread[%d] Callback Sub Message End...\n", i);
        }
    }
#endif

    EnterMutex();
    while (m_listSubMessageResource.size() > 0)
    {
        delete m_listSubMessageResource.front();
        m_listSubMessageResource.pop_front();
    }
    LeaveMutex();
    return;
}
bool CZeromqManage::AddSubMessage(const char * pSubMessage)
{
    bool bRet = false;
    if (NULL != m_pSubSocket)
    {
        if (zmq_setsockopt(m_pSubSocket, ZMQ_SUBSCRIBE, pSubMessage, strlen(pSubMessage)) < 0)
        {
            printf("****Error: zmq_setsockopt Subscribe[%s] Failed[%s]!\n", pSubMessage, zmq_strerror(errno));
        }
        else
        {
            //printf("##Sub [%s] success\n", pSubMessage); 
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "##Sub [%s] success.", pSubMessage);
            bRet = true;
        }
    }
    else
    {
        printf("***Warning: Sub Socket Not Initial!\n");
    }
    
    return bRet;
}
bool CZeromqManage::DelSubMessage(char * pSubMessage)
{
    bool bRet = false;
    if (NULL != m_pSubSocket)
    {
        if (zmq_setsockopt(m_pSubSocket, ZMQ_UNSUBSCRIBE, pSubMessage, strlen(pSubMessage)) < 0)
        {
            printf("****Error: zmq_setsockopt UnSubscribe[%s] Failed[%s]!\n", pSubMessage, zmq_strerror(errno));
        }
        else
        {
            printf("##ȡ������[%s]�ɹ�!\n", pSubMessage);
            bRet = true;
        }
    }
    else
    {
        printf("***Warning: Sub Socket Not Initial!\n");
    }
    return bRet;
}
#ifdef _WINDOWS_
DWORD CZeromqManage::SubMessageThread(void * pParam)
{
    CZeromqManage * pThis = (CZeromqManage *)pParam;
    pThis->SubMessageAction();
    return 0;
}
#else 
void * CZeromqManage::SubMessageThread(void * pParam)
{
    CZeromqManage * pThis = (CZeromqManage *)pParam;
    pThis->SubMessageAction();
    return NULL;
}
#endif
void CZeromqManage::SubMessageAction()
{
    char pRecvMsg[1024 * 20] = { 0 };
    while (!m_bStopFlag)
    {
        LPSUBMESSAGE pSubMessage = GetFreeResource();
        if (NULL == pSubMessage)
        {
            printf("***Warning: Pub Thread Cann't Get Free Resource!\n");
#ifdef _WINDOWS_
            Sleep(THREADWAITTIME * 100);
#else
            usleep(THREADWAITTIME * 100 * 1000);
#endif
            continue;
        }
        int nRet = zmq_recv(m_pSubSocket, pSubMessage->pHead, sizeof(pSubMessage->pHead), 0);       //����Ϣʱ������
        if(nRet < 0)
        {
            printf("****error: %s\n", zmq_strerror(errno));
            delete pSubMessage;
            zmq_close(m_pSubSocket);
            m_pSubSocket = NULL;
            return;
        }
        else
        {
            bool bRecv = false;
            
            int nMore = 0;
            size_t nMoreSize = sizeof(nMore);
            nRet = zmq_getsockopt(m_pSubSocket, ZMQ_RCVMORE, &nMore, &nMoreSize);
            if (nRet < 0)
            {
                printf("****error: %s\n", zmq_strerror(errno));
                delete pSubMessage;
                zmq_close(m_pSubSocket);
                m_pSubSocket = NULL;
                return;
            }
            else
            {
                if (nMore)
                {
                    nRet = zmq_recv(m_pSubSocket, pRecvMsg, sizeof(pRecvMsg), 0);
                    if (nRet < 0)
                    {
                        printf("****error: %s\n", zmq_strerror(errno));
                        delete pSubMessage;
                        zmq_close(m_pSubSocket);
                        m_pSubSocket = NULL;
                        return;
                    }
                    else
                    {
                        pSubMessage->sSubJsonValue.assign(pRecvMsg, nRet);
                        EnterMutex();
                        m_listSubMessageCallback.push_back(pSubMessage);
                        MsgNotify();
                        LeaveMutex();
                        bRecv = true;
                    }
                }
            }
            
            if(!bRecv)
            {
                printf("***Warning: Recv more Failed.\n");
                FreeResource(pSubMessage);
            }
        }
    }

    zmq_close(m_pSubSocket);
    m_pSubSocket = NULL;
    printf("Sub Thread End...\n");

    return;
}
#ifdef _WINDOWS_
DWORD CZeromqManage::MessageCallbackThread(void * pParam)
{
    CZeromqManage * pThis = (CZeromqManage *)pParam;
    pThis->MessageCallbackAction();
    return 0;
}
#else 
void * CZeromqManage::MessageCallbackThread(void * pParam)
{
    CZeromqManage * pThis = (CZeromqManage *)pParam;
    pThis->MessageCallbackAction();
    return NULL;
}
#endif
void CZeromqManage::MessageCallbackAction()
{
    char pPipeMsg[10] = { 0 };
    LPSUBMESSAGE pSubMessage = NULL;
    while (!m_bStopFlag)
    {
#ifdef _WINDOWS_
        /*if (WAIT_TIMEOUT == WaitForSingleObject(m_hMsgEvent, THREADWAITTIME))
        {
            continue;
        }*/
        WaitForSingleObject(m_hMsgEvent, INFINITE);
        ResetEvent(m_hMsgEvent);
#else
        read(m_nPipe[0], pPipeMsg, 1);
#endif
        do 
        {
            EnterMutex();
            if (m_listSubMessageCallback.size() > 0)
            {
                pSubMessage = m_listSubMessageCallback.front();
                m_listSubMessageCallback.pop_front();
            }
            else
            {
                pSubMessage = NULL;
            }
            LeaveMutex();
            if (NULL != pSubMessage)
            {
                m_pSubMsgCallback(pSubMessage, m_pUser);
                FreeResource(pSubMessage);
            }
        } while (NULL != pSubMessage);
    }
}
LPSUBMESSAGE CZeromqManage::GetFreeResource()
{
    LPSUBMESSAGE pSubMessage = NULL;
    EnterMutex();
    if (m_listSubMessageResource.size() > 0)
    {
        pSubMessage = m_listSubMessageResource.front();
        m_listSubMessageResource.pop_front();
    }
    else
    {
        pSubMessage = new SUBMESSAGE;
        if (NULL == pSubMessage)
        {
            printf("****Error: new Failed!\n");
        }
    }
    LeaveMutex();

    pSubMessage->Init();
    return pSubMessage;
}
void CZeromqManage::FreeResource(LPSUBMESSAGE pSubMessage)
{
    EnterMutex();
    m_listSubMessageResource.push_back(pSubMessage);
    //while (m_listSubMessageResource.size() > m_nThreadNum * 20)
    {
        //delete m_listSubMessageResource.front();
        //m_listSubMessageResource.pop_front();
    }
    LeaveMutex();
}
bool CZeromqManage::InitPub(char * pLocalIP, int nLocalPort, char * pServerIP, int nServerPort)
{
    if (NULL == m_pCtx)
    {
        if (NULL == (m_pCtx = zmq_ctx_new()))
        {
            printf("****Error: zmq_ctx_new Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    if (NULL == m_pPubSocket)
    {
        if (NULL == (m_pPubSocket = zmq_socket(m_pCtx, ZMQ_PUB)))
        {
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_socket Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    if (NULL != pLocalIP && 0 != nLocalPort)
    {
        char pAddress[32] = { 0 };
        sprintf(pAddress, "tcp://%s:%d", pLocalIP, nLocalPort);
        if (zmq_bind(m_pPushSocket, pAddress) < 0)
        {
            zmq_close(m_pPubSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: InitPub::zmq_bind[%s:%d] Failed[%s]!\n", pLocalIP, nLocalPort, zmq_strerror(errno));
            return false;
        }
    }
    if (NULL != pServerIP && 0 != nServerPort)
    {
        char pServerAddress[32] = { 0 };
        sprintf(pServerAddress, "tcp://%s:%d", pServerIP, nServerPort);
        if (zmq_connect(m_pPubSocket, pServerAddress) < 0)
        {
            zmq_close(m_pPubSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: InitPub::zmq_connect[%s:%d] Failed[%s]!\n", pServerIP, nServerPort, zmq_strerror(errno));
            return false;
        }
    }
        
    return true;
}

bool CZeromqManage::PubMessage(LPSUBMESSAGE pSubMessage)
{
    if (NULL != m_pPubSocket)
    {
        EnterPubMutex();
        zmq_send(m_pPubSocket, pSubMessage->pHead, strlen(pSubMessage->pHead), ZMQ_SNDMORE);
        zmq_send(m_pPubSocket, pSubMessage->pOperationType, strlen(pSubMessage->pOperationType), ZMQ_SNDMORE);
        zmq_send(m_pPubSocket, pSubMessage->pSource, strlen(pSubMessage->pSource), ZMQ_SNDMORE);
        zmq_send(m_pPubSocket, pSubMessage->sPubJsonValue.c_str(), pSubMessage->sPubJsonValue.size(), 0);
        LeavePubMutex();
    }
    else
    {
        printf("****Error: PubMessage Failed, NULL == m_pPubSocket!");
        return false;
    }
    
    return true;
}
bool CZeromqManage::PubMessage(const char * pHead, char * pID)
{
    if (NULL != m_pPubSocket)
    {
        EnterPubMutex();
        zmq_send(m_pPubSocket, pHead, strlen(pHead), ZMQ_SNDMORE);
        zmq_send(m_pPubSocket, pID, strlen(pID), 0);
        LeavePubMutex();
    }
    else
    {
        printf("****Error: PubMessage Failed, NULL == m_pPubSocket!");
        return false;
    }

    return true;
}
bool CZeromqManage::InitPush(char * pLocalIP, int nLocalPushPort, char * pServerIP, int nServerPort)
{
    if (NULL == m_pCtx)
    {
        if (NULL == (m_pCtx = zmq_ctx_new()))
        {
            printf("****Error: zmq_ctx_new Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    if (NULL == m_pPushSocket)
    {
        if (NULL == (m_pPushSocket = zmq_socket(m_pCtx, ZMQ_PUSH)))
        {
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_socket Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    if (NULL != pLocalIP && 0 != nLocalPushPort)
    {
        char pAddress[32] = { 0 };
        sprintf(pAddress, "tcp://%s:%d", pLocalIP, nLocalPushPort);
        if (zmq_bind(m_pPushSocket, pAddress) < 0)
        {
            zmq_close(m_pPushSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: InitPush::zmq_bind Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    if (NULL != pServerIP && 0 != nServerPort)
    {
        char pAddress[32] = { 0 };
        sprintf(pAddress, "tcp://%s:%d", pServerIP, nServerPort);
        if (zmq_connect(m_pPushSocket, pAddress) < 0)
        {
            zmq_close(m_pPushSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: InitPush::zmq_connect[%s:%d] Failed[%s]!\n",pServerIP, nServerPort, zmq_strerror(errno));
            return false;
        }
    }
    
    return true;
}
bool CZeromqManage::PushMessage(LPSUBMESSAGE pSubMessage)
{
    if (NULL != m_pPushSocket)
    {
        EnterPubMutex();
        zmq_send(m_pPushSocket, pSubMessage->pHead, strlen(pSubMessage->pHead), ZMQ_SNDMORE);
        zmq_send(m_pPushSocket, pSubMessage->pOperationType, strlen(pSubMessage->pOperationType), ZMQ_SNDMORE);
        zmq_send(m_pPushSocket, pSubMessage->pSource, strlen(pSubMessage->pSource), ZMQ_SNDMORE);
        zmq_send(m_pPushSocket, pSubMessage->sPubJsonValue.c_str(), pSubMessage->sPubJsonValue.size(), 0);
        LeavePubMutex();
    }
    else
    {
        printf("****Error: PushMessage Failed, NULL == m_pPushSocket!");
        return false;
    }

    return true;
}
bool CZeromqManage::InitPull(char * pLocalIP, int nLocalPushPort, char * pServerIP, int nServerPort,
                                LPSubMessageCallback pSubMessageCallback, void * pUser, int nThreadNum)
{
    m_pSubMsgCallback = pSubMessageCallback;
    m_pUser = pUser;
    m_nThreadNum = nThreadNum;

    if (NULL == m_pCtx)
    {
        if (NULL == (m_pCtx = zmq_ctx_new()))
        {
            printf("****Error: zmq_ctx_new Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }

    if (NULL == m_pPullSocket)
    {
        if (NULL == (m_pPullSocket = zmq_socket(m_pCtx, ZMQ_PULL)))
        {
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_socket Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }

    if (NULL != pLocalIP && 0 != nLocalPushPort)
    {
        char pAddress[32] = { 0 };
        sprintf(pAddress, "tcp://%s:%d", pLocalIP, nLocalPushPort);
        if (zmq_bind(m_pPullSocket, pAddress) < 0)
        {
            zmq_close(m_pPullSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_bind[%s:%d] Failed[%s]!\n", pLocalIP, nLocalPushPort, zmq_strerror(errno));
            return false;
        }
    }

    if (NULL != pServerIP && 0 != nServerPort)
    {
        char pAddress[32] = { 0 };
        sprintf(pAddress, "tcp://%s:%d", pServerIP, nServerPort);
        if (zmq_connect(m_pPullSocket, pAddress) < 0)
        {
            zmq_close(m_pPullSocket);
            zmq_ctx_destroy(m_pCtx);
            printf("****Error: zmq_connect Failed[%s]!\n", zmq_strerror(errno));
            return false;
        }
    }
    

    if (0 == m_listSubMessageResource.size())
    {
        for (int i = 0; i < m_nThreadNum * 20; i++)
        {
            LPSUBMESSAGE pSubMessage = new SUBMESSAGE;
            m_listSubMessageResource.push_back(pSubMessage);
        }
    }
#ifdef _WINDOWS_
    if (INVALID_HANDLE_VALUE == m_ThreadPullMessageID)
    {
        m_ThreadPullMessageID = CreateThread(NULL, 0, PullMessageThread, this, NULL, 0);   //�����ȶ��߳�
    }
    for (int i = 0; i < m_nThreadNum; i++)
    {
        if (INVALID_HANDLE_VALUE == m_ThreadCallbackID[i])
        {
            m_ThreadCallbackID[i] = CreateThread(NULL, 0, MessageCallbackThread, this, NULL, 0);   //�����ȶ��߳�
        }
    }
#else    
    if (-1 == m_ThreadPullMessageID)
    {
        pthread_create(&m_ThreadPullMessageID, NULL, PullMessageThread, (void*)this);
    }
    for (int i = 0; i < m_nThreadNum; i++)
    {
        if (-1 == m_ThreadCallbackID[i])
        {
            pthread_create(&m_ThreadCallbackID[i], NULL, MessageCallbackThread, (void*)this);
        }
    }
#endif
    return true;
}
#ifdef _WINDOWS_
DWORD CZeromqManage::PullMessageThread(void * pParam)
{
    CZeromqManage * pThis = (CZeromqManage *)pParam;
    pThis->PullMessageAction();
    return 0;
}
#else 
void * CZeromqManage::PullMessageThread(void * pParam)
{
    CZeromqManage * pThis = (CZeromqManage *)pParam;
    pThis->PullMessageAction();
    return NULL;
}
#endif
void CZeromqManage::PullMessageAction()
{
    char pRecvMsg[1024 * 20] = { 0 };
    while (!m_bStopFlag)
    {
        LPSUBMESSAGE pSubMessage = GetFreeResource();
        if (NULL == pSubMessage)
        {
            printf("***Warning: Pull Thread Cann't Get Free Resource!\n");
#ifdef _WINDOWS_
            Sleep(THREADWAITTIME * 100);
#else
            usleep(THREADWAITTIME * 100 * 1000);
#endif
            continue;
        }
        int nRet = zmq_recv(m_pPullSocket, pSubMessage->pHead, sizeof(pSubMessage->pHead), 0);       //����Ϣʱ������
        if (nRet < 0)
        {
            printf("****error: %s\n", zmq_strerror(errno));
            delete pSubMessage;
            zmq_close(m_pPullSocket);
            m_pPullSocket = NULL;
            return;
        }
        else
        {
            bool bRecv = false;

            int nMore = 0;
            size_t nMoreSize = sizeof(nMore);
            zmq_getsockopt(m_pPullSocket, ZMQ_RCVMORE, &nMore, &nMoreSize);
            if (nMore)
            {
                zmq_recv(m_pPullSocket, pSubMessage->pOperationType, sizeof(pSubMessage->pOperationType), 0);
                zmq_setsockopt(m_pPullSocket, ZMQ_RCVMORE, &nMore, nMoreSize);
                if (nMore)
                {
                    zmq_recv(m_pPullSocket, pSubMessage->pSource, sizeof(pSubMessage->pSource), 0);
                    //printf("\n------------Recv: \n");
                    //printf("%s\n%s\n%s\n------------\n", pSubMessage->pHead, pSubMessage->pOperationType, pSubMessage->pSource);
                    zmq_setsockopt(m_pPullSocket, ZMQ_RCVMORE, &nMore, nMoreSize);
                    if (nMore)
                    {
                        nRet = zmq_recv(m_pPullSocket, pRecvMsg, sizeof(pRecvMsg), 0);
                        pSubMessage->sSubJsonValue.assign(pRecvMsg, nRet);
                        EnterMutex();
                        m_listSubMessageCallback.push_back(pSubMessage);
                        MsgNotify();
                        LeaveMutex();

                        bRecv = true;
                    }
                }
            }

            if (!bRecv)
            {
                printf("***Warning: Recv more Failed.\n");
                FreeResource(pSubMessage);
            }
        }
    }
    return;
}
//�����ٽ���
void CZeromqManage::EnterMutex()
{
#ifdef _WINDOWS_
    EnterCriticalSection(&m_cs);
#else
    pthread_mutex_lock(&m_mutex);
#endif
}
//�˳��ٽ���
void CZeromqManage::LeaveMutex()
{
#ifdef _WINDOWS_
    LeaveCriticalSection(&m_cs);
#else
    pthread_mutex_unlock(&m_mutex);
#endif
}
void CZeromqManage::MsgNotify()
{
#ifdef _WINDOWS_
    SetEvent(m_hMsgEvent);
#else
    write(m_nPipe[1], "1", 1);
#endif
}
//�����ٽ���
void CZeromqManage::EnterPubMutex()
{
#ifdef _WINDOWS_
    EnterCriticalSection(&m_Pubcs);
#else
    pthread_mutex_lock(&m_PubMutex);
#endif
}
//�˳��ٽ���
void CZeromqManage::LeavePubMutex()
{
#ifdef _WINDOWS_
    LeaveCriticalSection(&m_Pubcs);
#else
    pthread_mutex_unlock(&m_PubMutex);
#endif
}