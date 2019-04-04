#include "stdafx.h"
#include "SnapCollectService.h"

CLogRecorder g_LogRecorder;
CSnapCollectService::CSnapCollectService()
{
    m_bStopService = false;
    m_pMysqlOperation = NULL;
    m_pZeromqManage = NULL;
    m_pRedisManage = NULL;

    m_nServiceID = 0;
    m_sServiceIP = "";
    m_nServicePort = 0;
    m_sRedisIP = "";
    m_nRedisPort = 0;
    m_sZmqIP = "";
    m_nZmqPort = 0;
    m_vSTServerInfo.clear();
    m_nThreadCount = 4;
    //初始化临界区
#ifdef __WINDOWS__
    InitializeCriticalSection(&m_cs);
    m_hStopEvent = CreateEvent(NULL, true, false, NULL);
    m_hSubMsgEvent = CreateEvent(NULL, true, false, NULL);
    for (int i = 0; i < MAXTHREADCOUNT; i++)
    {
        m_hThreadSubMsgCommand[i] = INVALID_HANDLE_VALUE;
    }
    m_hThreadGetRelativeCamera = INVALID_HANDLE_VALUE;
#else
    pthread_mutex_init(&m_mutex, NULL);
    pipe(m_nPipeStop);
    pipe(m_nPipeSubMsg);
    for (int i = 0; i < MAXTHREADCOUNT; i++)
    {
        m_hThreadSubMsgCommand[i] = -1;
    }
    m_hThreadGetRelativeCamera = -1;
#endif
    m_nTotalSnapCount = 0;
}


CSnapCollectService::~CSnapCollectService()
{
#ifdef __WINDOWS__
    DeleteCriticalSection(&m_cs);
    CloseHandle(m_hStopEvent);
    CloseHandle(m_hSubMsgEvent);
#else
    pthread_mutex_destroy(&m_mutex);
    close(m_nPipeStop[0]);
    close(m_nPipeStop[1]);
    close(m_nPipeSubMsg[0]);
    close(m_nPipeSubMsg[1]);
#endif
}
bool CSnapCollectService::StartService()
{
    if (!Init())
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "XSSnapCollectService Init Failed!\n");
        return false;
    }

#ifdef __WINDOWS__
    Sleep(1000);
#else
    usleep(1000 * 1000);
#endif
    g_LogRecorder.WriteDebugLog(__FUNCTION__, "******************XSSnapCollectService Start******************\n\n");

#ifdef __WINDOWS__
#ifdef _DEBUG
    unsigned char pIn;
    while (true)
    {
        printf("\nInput: \n"
            "a: Show Info\n"
            "e: quit Service\n"
        );
        cin >> pIn;
        switch (pIn)
        {
        case 'a':
        {
            EnterMutex();
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Total: %d, Current: %d, Resource: %d.\n",
                m_nTotalSnapCount, m_listPubMessage.size(), m_listSubMessageResource.size());
            LeaveMutex();
            break;
        }
        case 'e':
        {
            StopService();
            return true;
        }
        default:
            break;
        }
        Sleep(1000);
    }
#else
    WaitForSingleObject(m_hStopEvent, INFINITE);
#endif
#else
    unsigned char pIn;
    while (true)
    {
        printf("\nInput: \n"
            "a: Show Info\n"
            "e: quit Service\n"
        );
        cin >> pIn;
        switch (pIn)
        {
        case 'a':
        {

            break;
        }
        case 'e':
        {
            StopService();
            return true;
        }
        default:
            break;
        }
        sleep(1);
    }
#endif
    return true;
}
bool CSnapCollectService::StopService()
{
    m_bStopService = true;
#ifdef __WINDOWS__
    SetEvent(m_hStopEvent);
    for (int i = 0; i < m_nThreadCount; i++)
    {
        SetEvent(m_hSubMsgEvent);
        Sleep(20);
    }
    for (int i = 0; i < m_nThreadCount; i++)
    {
        if (INVALID_HANDLE_VALUE != m_hThreadSubMsgCommand[i])
        {
            WaitForSingleObject(m_hThreadSubMsgCommand[i], INFINITE);
        }
    }
        
    if (INVALID_HANDLE_VALUE != m_hThreadGetRelativeCamera)
    {
        WaitForSingleObject(m_hThreadGetRelativeCamera, INFINITE);
    }
#else
    write(m_nPipeStop[1], "1", 1);
    for (int i = 0; i < m_nThreadCount; i++)
    {
        write(m_nPipeSubMsg[1], "1", 1);
    }
    for (int i = 0; i < m_nThreadCount; i++)
    {
        if (-1 != m_hThreadSubMsgCommand[i])
        {
            pthread_join(m_hThreadSubMsgCommand[i], NULL);
        }
    }
      
    if (-1 != m_hThreadGetRelativeCamera)
    {
        pthread_join(m_hThreadGetRelativeCamera, NULL);
    }
#endif
    printf("--Thread End.\n");

    if (NULL != m_pZeromqManage)
    {
        m_pZeromqManage->UnInit();
        delete m_pZeromqManage;
        m_pZeromqManage = NULL;
    }
    if (NULL != m_pRedisManage)
    {
        m_pRedisManage->UnInit();
        delete m_pRedisManage;
        m_pRedisManage = NULL;
    }
    if (NULL != m_pMysqlOperation)
    {
        m_pMysqlOperation->DisConnectDB();
        delete m_pMysqlOperation;
        m_pMysqlOperation = NULL;
    }

    printf("************************Stop SnapCollectService************************\n");
    return true;
}
bool CSnapCollectService::Init()
{
#ifdef __WINDOWS__
    string sPath = m_ConfigRead.GetCurrentPath();
    string sConfigPath = sPath + "/Config/XSSnapCollectService_config.properties";
    g_LogRecorder.InitLogger(sConfigPath.c_str(), "XSSnapCollectServiceLogger", "XSSnapCollectService");
#endif
    if (!m_ConfigRead.ReadConfig())
    {
        printf("****Error: Read Config File Failed.\n");
        return false;
    }
    if (!InitDB())
    {
        printf("***Warning: connect MySQL Failed.\n");
        return false;
    }
    //初始化Zeromq
    if (!InitZeromq())
    {
        return false;
    }
    //初始化Redis连接
    if (!InitRedis())
    {
        return false;
    }

    strcpy(m_pImageAddress[0], IMAGEADDRESS1);
    strcpy(m_pImageAddress[1], IMAGEADDRESS2);
    strcpy(m_pImageAddress[2], IMAGEADDRESS3);
    strcpy(m_pImageAddress[3], IMAGEADDRESS4);

    //初始化资源池
    for (int i = 0; i < m_ConfigRead.m_nThreadCount * m_vSTServerInfo.size() * 50; i++)
    {
        LPSUBMESSAGE pSubMessage = new SUBMESSAGE;
        m_listSubMessageResource.push_back(pSubMessage);
    }
    
    m_nThreadCount = (m_ConfigRead.m_nThreadCount * m_vSTServerInfo.size());
    m_nThreadCount = m_nThreadCount < MAXTHREADCOUNT ? m_nThreadCount : MAXTHREADCOUNT;

    //m_nThreadCount = 1;
    //获取当前采集关联镜头线程
#ifdef __WINDOWS__
    for (int i = 0; i < m_nThreadCount; i++)
    {
        m_hThreadSubMsgCommand[i] = CreateThread(NULL, 0, SubMessageCommandThread, this, NULL, 0);
        Sleep(20);
    }
    Sleep(1000);
    m_hThreadGetRelativeCamera = CreateThread(NULL, 0, GetRelativeCameraThread, this, NULL, 0);
#else
    for (int i = 0; i < m_nThreadCount; i++)
    {
        pthread_create(&m_hThreadSubMsgCommand[i], NULL, SubMessageCommandThread, (void*)this);
        usleep(1000 * 20);
    }
    sleep(1);
    pthread_create(&m_hThreadGetRelativeCamera, NULL, GetRelativeCameraThread, (void*)this);
#endif
    printf("--Thread Start.\n");
    return true;
}
bool CSnapCollectService::InitDB()
{
    if (NULL == m_pMysqlOperation)
    {
        m_pMysqlOperation = new CMySQLOperation();
    }
    if (!m_pMysqlOperation->ConnectDB(m_ConfigRead.m_sDBIP.c_str(), m_ConfigRead.m_sDBUser.c_str(),
        m_ConfigRead.m_sDBPd.c_str(), m_ConfigRead.m_sDBName.c_str(), m_ConfigRead.m_nDBPort))
    {
        return false;
    }

    char pSQL[SQLMAXLEN] = { 0 };
    sprintf(pSQL, "select ID, servercode, servertype, ip, port from %s", SERVERINFOTABLE);

    MYSQL_RES * result = NULL;
    if (!m_pMysqlOperation->GetQueryResult(pSQL, result))
    {
        return false;
    }

    int nRowCount = mysql_num_rows(result);
    int nNum = 1;
    if (nRowCount > 0)
    {
        int nServerType = 0;
        MYSQL_ROW row = NULL;
        row = mysql_fetch_row(result);
        while (NULL != row)
        {
            if (NULL == row[0] || NULL == row[2] || NULL == row[3] || NULL == row[4])
            {
                g_LogRecorder.WriteDebugLog(__FUNCTION__, "ServerInfo Table Value is Null!\n");
            }
            else
            {
                nServerType = atoi(row[2]);
                switch (nServerType)
                {
                case 240:   //采集服务自身
                {
                    if (strcmp(m_ConfigRead.m_sServerID.c_str(), row[1]) == 0)
                    {
                        m_nServiceID = atoi(row[0]);
                        m_sServiceIP = row[3];
                        m_nServicePort = atoi(row[4]);
                    }
                    break;
                }
                case 235:   //Redis
                {
                    m_sRedisIP = row[3];
                    m_nRedisPort = atoi(row[4]);
                    break;
                }
                case 255:   //zeromq proxy service
                {
                    m_sZmqIP = row[3];
                    m_nZmqPort = atoi(row[4]);
                    break;
                }
                case 254:   //商汤服务器
                {
                    LPSTSERVERINFO pSTInfo = new STSERVERINFO;
                    strcpy(pSTInfo->pIP, row[3]);
                    pSTInfo->nPort = atoi(row[4]);
                    m_vSTServerInfo.push_back(pSTInfo);
                    break;
                }
                default:
                    break;
                }
            }
            row = mysql_fetch_row(result);
        }
    }
    mysql_free_result(result);

    if (m_nServiceID == 0 || m_sServiceIP == "" || m_nServicePort == 0 ||
        m_sRedisIP == "" || m_nRedisPort == 0 ||
        m_sZmqIP == "" || m_nZmqPort == 0 ||
        m_vSTServerInfo.size() == 0)
    {
        return false;
    }

    return true;
}
bool CSnapCollectService::InitRedis()
{
    if (NULL == m_pRedisManage)
    {
        m_pRedisManage = new CRedisManage;
    }
    if (!m_pRedisManage->InitRedis((char*)m_sRedisIP.c_str(), m_nRedisPort))
    {
        return false;
    }
    return true;
}
bool CSnapCollectService::InitZeromq()
{
    if (NULL == m_pZeromqManage)
    {
        m_pZeromqManage = new CZeromqManage;
    }
    if (!m_pZeromqManage->InitSub(NULL, 0, (char*)m_sZmqIP.c_str(), m_nZmqPort, ZeromqSubMsg, this, 1))
    {
        printf("****Error: Init ZMQ[%s:%d]失败!", m_sZmqIP.c_str(), m_nZmqPort);
        return false;
    }
    if (!m_pZeromqManage->InitPub(NULL, 0, (char*)m_sZmqIP.c_str(), m_nZmqPort + 1))
    {
        printf("****Error: Init ZMQ[%s:%d]失败!", m_sZmqIP.c_str(), m_nZmqPort + 1);
        return false;
    }
    return true;
}
#ifdef __WINDOWS__
DWORD WINAPI CSnapCollectService::GetRelativeCameraThread(void * pParam)
{
    CSnapCollectService * pThis = (CSnapCollectService *)pParam;
    pThis->GetRelativeCameraAction();
    return 0;
}
#else
void * CSnapCollectService::GetRelativeCameraThread(void * pParam)
{
    CSnapCollectService * pThis = (CSnapCollectService *)pParam;
    pThis->GetRelativeCameraAction();
    return NULL;
}
#endif
void CSnapCollectService::GetRelativeCameraAction()
{
    char pSQL[SQLMAXLEN] = { 0 };
    sprintf(pSQL, "select a.deviceid from %s a, %s b where b.CollectionID = %d and a.id = b.IpcameraID", 
                    IPCAMERATABLE, RELATIVESERVERTABLE, m_nServiceID);
    set<string>::iterator it = m_setRelativeCamera.begin();
    char pSub[1024] = { 0 };

    //商汤通讯指针
    CHttpSyncClientPtr pClientST(NULL);
    char pSTURL[128] = { 0 };
    LPCBYTE pRespon = NULL;
    int nRepSize = 0;

    THeader * tHeader = new THeader[6];
    tHeader[0].name = "Host";
    char pHost[32] = { 0 };
    tHeader[0].value = pHost;
    tHeader[1].name = "Connection";
    tHeader[1].value = "keep-alive";
    tHeader[2].name = "Content-Type";
    tHeader[2].value = "application/x-www-form-urlencoded; charset=UTF-8";
    tHeader[3].name = "Accept-Language";
    tHeader[3].value = "zh-CN,zh;q=0.9";
    tHeader[4].name = "Accept-Encoding";
    tHeader[4].value = "gzip, deflate";
    tHeader[5].name = "Content-Length";

    char pHttpBody[] = { "dbName=CollectTemp" };
    int nBodyLen = strlen(pHttpBody);

    char pBodyLen[128] = { 0 };
    sprintf_s(pBodyLen, sizeof(pBodyLen), "%d", nBodyLen);
    tHeader[5].value = pBodyLen;

    time_t t = time(NULL);
    tm * tCurrent = localtime(&t);
    int nDay = tCurrent->tm_mday;

    while (true)
    {
        //获取订阅镜头编号, 开始订阅
        MYSQL_RES * result = NULL;
        if (!m_pMysqlOperation->GetQueryResult(pSQL, result))
        {
            return;
        }

        int nRowCount = mysql_num_rows(result);
        int nNum = 1;
        if (nRowCount > 0)
        {
            int nServerType = 0;
            MYSQL_ROW row = NULL;
            row = mysql_fetch_row(result);
            while (NULL != row)
            {
                it = m_setRelativeCamera.find(row[0]);
                if (m_setRelativeCamera.end() == it)
                {
                    m_setRelativeCamera.insert(row[0]);
                    sprintf(pSub, "nwkafka.%s", row[0]);
                    m_pZeromqManage->AddSubMessage(pSub);
                }
                row = mysql_fetch_row(result);
            }
        }
        mysql_free_result(result);

        EnterMutex();
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Total Snap: %d, Current: %d, Resource: %d.",
            m_nTotalSnapCount, m_listPubMessage.size(), m_listSubMessageResource.size());
        LeaveMutex();


        time_t t = time(NULL);
        tm * tCurrent = localtime(&t);
        if (nDay != tCurrent->tm_mday)
        {
            nDay = tCurrent->tm_mday;

            //定时清空ST服务器临时采集存储库
            vector<LPSTSERVERINFO>::iterator it = m_vSTServerInfo.begin();
            for (; it != m_vSTServerInfo.end(); it++)
            {
                sprintf(pHost, "%s:%d", (*it)->pIP, (*it)->nPort);
                sprintf(pSTURL, "http://%s:%d/verify/target/clear", (*it)->pIP, (*it)->nPort);
                pClientST->OpenUrl("POST", pSTURL, tHeader, 6, (LPCBYTE)pHttpBody, nBodyLen);

                pClientST->GetResponseBody(&pRespon, &nRepSize);
                if (string((char*)pRespon, nRepSize).find("success") != string::npos)
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Clear ST[%s] Lib Success", pHost);
                }
                else
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "***Warning: Clear ST[%s] Lib Failed", pHost);
                }
            }
        }

#ifdef __WINDOWS__
        if (WAIT_OBJECT_0 == WaitForSingleObject(m_hStopEvent, 300 * 1000))
        {
            break;
        }
#else
        //等待, 无信号则下一次循环, 有信号(只有需要退出线程时才有信号)则退出线程
        tv.tv_sec = 300;
        tv.tv_usec = 0;
        fd_set fdPipe = fdsr;
        if (0 != select(m_nPipeStop[0] + 1, &fdPipe, NULL, NULL, &tv)) //阻塞 
        {
            break;
        }
#endif
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "GetRelativeCameraAction Thread End....");

    return;
}
//Zeromq消息回调
void CSnapCollectService::ZeromqSubMsg(LPSUBMESSAGE pSubMessage, void * pUser)
{
    CSnapCollectService * pThis = (CSnapCollectService *)pUser;
    LPSUBMESSAGE pMessage = pThis->GetFreeResource();

    //复制
    strcpy(pMessage->pHead, pSubMessage->pHead);
    pMessage->sSubJsonValue = pSubMessage->sSubJsonValue;
    //放入list
    pThis->EnterMutex();
    pThis->m_listPubMessage.push_back(pMessage);
    pThis->m_nTotalSnapCount++;
#ifdef __WINDOWS__
    SetEvent(pThis->m_hSubMsgEvent);
#else
    write(pThis->m_nPipeSubMsg[1], "1", 1);
#endif
    pThis->LeaveMutex();

    return;
}
#ifdef __WINDOWS__
DWORD WINAPI CSnapCollectService::SubMessageCommandThread(void * pParam)
{
    CSnapCollectService * pThis = (CSnapCollectService *)pParam;
    pThis->SubMessageCommandAction();
    return 0;
}
#else
void * CSnapCollectService::SubMessageCommandThread(void * pParam)
{
    CLibInfo * pThis = (CLibInfo *)pParam;
    pThis->PubMessageCommandAction();
    return NULL;
}
#endif
//从list取出submessage处理
int nThreadOrder = 0;
void CSnapCollectService::SubMessageCommandAction()
{
    //分配ST服务器
    int nCurrentThreadOrder = nThreadOrder++;
    int nSTOrder = nCurrentThreadOrder % (m_vSTServerInfo.size());
    char pSTURL[128] = { 0 };
    sprintf(pSTURL, "http://%s:%d/verify/face/synAdd", m_vSTServerInfo[nSTOrder]->pIP, m_vSTServerInfo[nSTOrder]->nPort);

    //云存地址更换
    int nImageAddress = nCurrentThreadOrder % 4;

    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Current Thread[%d]: \n[ST: %s]\n[ImageIP: %s]",
                    nCurrentThreadOrder, pSTURL, m_pImageAddress[nImageAddress]);


    char pPipeRead[10] = { 0 };
    LPSUBMESSAGE pSubMessage = NULL;
    bool bTask = false;

    //用来根据JSON串解析到的faceurl获取人脸图片
    CHttpSyncClientPtr pClient(NULL);
    //如有重定向, 由这个指针来获取重定向人脸图片
    CHttpSyncClientPtr pClientLocation(NULL);
    //向商汤取特征值指针
    CHttpSyncClientPtr pClientST(NULL);
    LPCBYTE pRespon = NULL;
    int nRepSize = 0;
    //HTTP重定向之后的URL
    LPCSTR pLocation = NULL;
    //是否取图片成功
    bool bGetImage = false;

    //向ST发送取特征值HTTP消息头数据
    THeader * tHeader = new THeader[6];
    tHeader[0].name = "Host";
    char pHost[32] = { 0 };
    sprintf(pHost, "%s:%d", m_vSTServerInfo[nSTOrder]->pIP, m_vSTServerInfo[nSTOrder]->nPort);
    tHeader[0].value = pHost;
    tHeader[1].name = "Connection";
    tHeader[1].value = "keep-alive";
    tHeader[2].name = "Content-Type";
    tHeader[2].value = "multipart/form-data; boundary=----WebKitFormBoundaryw3scyHKZuywS9NlA";
    tHeader[3].name = "Accept-Language";
    tHeader[3].value = "zh-CN,zh;q=0.9";
    tHeader[4].name = "Accept-Encoding";
    tHeader[4].value = "gzip, deflate";
    tHeader[5].name = "Content-Length";
    //HTTP Body
    char * pHttpBody = new char[FACEIMAGELEN];
    int nBodyLen = 0;
    char pBoundary[] = "------WebKitFormBoundaryw3scyHKZuywS9NlA";

    while (!m_bStopService)
    {
#ifdef __WINDOWS__
        WaitForSingleObject(m_hSubMsgEvent, INFINITE);
        if (m_bStopService)
        {
            break;
        }
        ResetEvent(m_hSubMsgEvent);
#else
        read(m_nPipeSubMsg[0], pPipeRead, 1);
        if (m_bStopService)
        {
            break;
        }
#endif
        do
        {
            EnterMutex();
            if (m_listPubMessage.size() > 0)
            {
                pSubMessage = m_listPubMessage.front();
                m_listPubMessage.pop_front();
                bTask = true;
            }
            else
            {
                pSubMessage = NULL;
                bTask = false;
            }
            LeaveMutex();
            if (NULL != pSubMessage)
            {
                //处理订阅消息
                printf("Msg: [%02d]%s\n", nCurrentThreadOrder, pSubMessage->pHead);
                //printf("SubMsg::%s\n%s\n", pSubMessage->pHead, pSubMessage->pSubJsonValue);
                //1. 解析订阅json
                if (!ParseZmqJson(pSubMessage, nImageAddress))
                {
                    FreeResource(pSubMessage);
                    continue;
                }
                //2. 获取图片
                int nErrorCount = 1;    //send data failed count, >3抛弃
                do
                {
                    bGetImage = false;
                    bool bRet = pClient->OpenUrl("GET", pSubMessage->pFaceURL);
                    if (bRet)
                    {
                        USHORT uStatus = pClient->GetStatusCode();
                        if (200 == uStatus)         //取图片成功
                        {
                            pClient->GetResponseBody(&pRespon, &nRepSize);
                            if (nRepSize > 0)
                            {
                                bGetImage = true;
                            }
                            else
                            {
                                nErrorCount++;
                                if (nErrorCount > 3)
                                {
                                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d][%s][%d]URL[%s] Get Image Size = 0!",
                                        nCurrentThreadOrder, pSubMessage->pDeviceID, nErrorCount, pSubMessage->pFaceURL);
                                    break;
                                }
                                else
                                {
                                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d][%s][%d]URL[%s] Get Image Size = 0!",
                                        nCurrentThreadOrder, pSubMessage->pDeviceID, nErrorCount, pSubMessage->pFaceURL);
                                    Sleep(1000);
                                }
                            }
                        }
                        else if (301 == uStatus)    //图片重定向
                        {
                            pClient->GetHeader("Location", &pLocation);
                            //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d]URL Location: %s", nCurrentThreadOrder, pLocation);

                            bRet = pClientLocation->OpenUrl("GET", pLocation);
                            strcpy(pSubMessage->pFaceURL, pLocation);
                            uStatus = pClientLocation->GetStatusCode();
                            if (200 == uStatus)     //图片重定向后, 取图片成功
                            {
                                pClientLocation->GetResponseBody(&pRespon, &nRepSize);
                                if (nRepSize > 0)
                                {
                                    bGetImage = true;
                                }
                                else
                                {
                                    nErrorCount++;
                                    if (nErrorCount > 3)
                                    {
                                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d][%s]URL[%s] Get Image Size = 0!",
                                            nCurrentThreadOrder, pSubMessage->pDeviceID, pSubMessage->pFaceURL);
                                        break;
                                    }
                                }
                            }
                            else if (301 == uStatus)    //图片继续重定向, 则重新循环
                            {
                                pClientLocation->GetHeader("Location", &pLocation);
                                strcpy(pSubMessage->pFaceURL, pLocation);
                                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d]URL Location Continue: %s", 
                                    nCurrentThreadOrder, pLocation);
                            }
                            else
                            {
                                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d]OpenUrl Success, Get[%s] Failed, ErrorCode[%d].", 
                                    nCurrentThreadOrder, pLocation, uStatus);
                                break;
                            }
                        }
                        else
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d]OpenUrl Success, Get[%s] Failed, ErrorCode[%d].", 
                                nCurrentThreadOrder, pSubMessage->pFaceURL, uStatus);
                            break;
                        }
                    }
                    else
                    {
                        nErrorCount++;
                        if (nErrorCount > 3)
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d][%d]OpenUrl [%s] Failed, ErrorCode[%d][%s].",
                                nCurrentThreadOrder, nErrorCount++, pSubMessage->pFaceURL, pClient->GetLastError(), pClient->GetLastErrorDesc());
                            break;
                        }
                    }
                } while (!bGetImage);
                //如果取图片失败
                if (!bGetImage)
                {
                    FreeResource(pSubMessage);
                    continue;
                }

                //3. 向商汤服务器获取图片特征值及质量分数
                memset(pHttpBody, 0, FACEIMAGELEN);
                nBodyLen = 0;

                sprintf(pHttpBody,
                    "%s\r\n"
                    "Content-Disposition: form-data; name=\"dbName\"\r\n\r\n"
                    "%s\r\n"
                    "%s\r\n"
                    "Content-Disposition: form-data; name=\"getFeature\"\r\n\r\n"
                    "%d\r\n"
                    "%s\r\n"
                    "Content-Disposition: form-data; name=\"imageDatas\"; filename=\"%s.jpg\"\r\n"
                    "Content-Type: image/jpeg\r\n\r\n"
                    , pBoundary, "CollectTemp", pBoundary, 0, pBoundary, "123");
                nBodyLen += strlen(pHttpBody);

                memcpy(pHttpBody + nBodyLen, pRespon, nRepSize);
                nBodyLen += nRepSize;

                char pTail[128] = { 0 };
                sprintf_s(pTail, sizeof(pTail), "\r\n%s--\r\n", pBoundary);
                memcpy(pHttpBody + nBodyLen, pTail, strlen(pTail));
                nBodyLen += strlen(pTail);

                char pBodyLen[12] = { 0 };
                sprintf_s(pBodyLen, sizeof(pBodyLen), "%d", nBodyLen);
                tHeader[5].value = pBodyLen;

                bool bRet = pClientST->OpenUrl("POST", pSTURL, tHeader, 6, (LPCBYTE)pHttpBody, nBodyLen);
                if (bRet)
                {
                    USHORT uStatus = pClientST->GetStatusCode();
                    if (200 == uStatus)
                    {
                        pClientST->GetResponseBody(&pRespon, &nRepSize);
                    }
                    else
                    {
                        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "[%02d]OpenUrl Success, Get Feature Failed, Status: %d.\nface_url: %s", 
                            nCurrentThreadOrder, uStatus, pSubMessage->pFaceURL);
                        FreeResource(pSubMessage);
                        continue;
                    }
                }
                else
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d]OpenUrl [%s] Failed, ErrorCode[%d][%s].", 
                        nCurrentThreadOrder, pSTURL, pClientST->GetLastError(), pClientST->GetLastErrorDesc());
                    FreeResource(pSubMessage);
                    continue;
                }

                //4. 解析ST返回的JSON信息
                if (!ParseSTResponJson(pSubMessage, string((char*)pRespon, nRepSize)))
                {
                    FreeResource(pSubMessage);
                    continue;
                }
                //5. 向zmq代理发送图片及特征值信息, 由分析服务接收
                if (!SendImageInfo(pSubMessage))
                {
                    FreeResource(pSubMessage);
                    continue;
                }

                //6. 释放资源
                FreeResource(pSubMessage);
            }
        } while (bTask && !m_bStopService);
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "[%02d]SubMessageCommandThread Thread End....", nCurrentThreadOrder);
    return;
}
//1. 解析订阅json
bool CSnapCollectService::ParseZmqJson(LPSUBMESSAGE pSubMessage, int nImageAddress)
{
    bool bRet = false;
    rapidjson::Document document;
    //如果订阅json串为空, 直接跳过不处理
    if (pSubMessage->sSubJsonValue != "")
    {
        //解析json串
        document.Parse(pSubMessage->sSubJsonValue.c_str());
        //解析json串失败, 不处理
        if (document.HasParseError())
        {
            g_LogRecorder.WriteWarnLogEx(__FUNCTION__,
                "***Warning: Parse Json Format Failed[%s].", pSubMessage->sSubJsonValue.c_str());
        }
        else
        {
            //判断json串是否有指定字段
            if (document.HasMember(KAFKADEVICEID) && document[KAFKADEVICEID].IsString() && strlen(document[KAFKADEVICEID].GetString()) < MAXLEN &&
                document.HasMember(KAFKAUNIXTIME) && document[KAFKAUNIXTIME].IsInt64() &&
                //document.HasMember(KAFKASNAPTIME) && document[KAFKASNAPTIME].IsString() && strlen(document[KAFKASNAPTIME].GetString()) < MAXLEN &&
                document.HasMember(KAFKAFACEURL) && document[KAFKAFACEURL].IsString() && strlen(document[KAFKAFACEURL].GetString()) < 2048 &&
                document.HasMember(KAFKABKGURL) && document[KAFKABKGURL].IsString() && strlen(document[KAFKABKGURL].GetString()) < 2048)
            {
                strcpy(pSubMessage->pDeviceID, document[KAFKADEVICEID].GetString());
                pSubMessage->nImageTime = document[KAFKAUNIXTIME].GetInt64();
                /*string sSnapTime = document[KAFKASNAPTIME].GetString();
                int nTime = ChangeTimeToSecond(sSnapTime);
                pSubMessage->nImageTime = (int64_t)nTime * 1000;*/
                string sFaceURL = document[KAFKAFACEURL].GetString();
                string sBkgURL = document[KAFKABKGURL].GetString();

                if (sFaceURL.find("4.100") != string::npos && nImageAddress % 4 != 3)
                {
                    size_t nPos = sFaceURL.find(":", 10);
                    if (nPos != string::npos)
                    {
                        sFaceURL.replace(7, nPos - 7, m_pImageAddress[nImageAddress]);
                        strcpy(pSubMessage->pFaceURL, sFaceURL.c_str());

                        nPos = sBkgURL.find(":", 10);
                        if (nPos != string::npos)
                        {
                            sBkgURL.replace(7, nPos - 7, m_pImageAddress[nImageAddress]);
                            strcpy(pSubMessage->pBkgURL, sBkgURL.c_str());
                            bRet = true;
                        }
                        else
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "bkgurl[%s] format wrong!", sBkgURL.c_str());
                        }
                    }
                    else
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "faceurl[%s] format wrong!", sFaceURL.c_str());
                    }
                }
                else
                {
                    strcpy(pSubMessage->pFaceURL, sFaceURL.c_str());
                    strcpy(pSubMessage->pBkgURL, sBkgURL.c_str());
                    bRet = true;
                }
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Json Format Wrong[%s].", pSubMessage->sSubJsonValue.c_str());
            }
        }
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Sub [%s] Json = NULL!", pSubMessage->pHead);
    }
    
    return bRet;
}
//4. 解析ST返回的JSON信息
bool CSnapCollectService::ParseSTResponJson(LPSUBMESSAGE pSubMessage, string sRespon)
{
    bool bRet = false;
    rapidjson::Document document;
    //如果订阅json串为空, 直接跳过不处理
    if (sRespon != "")
    {
        //解析json串
        document.Parse(sRespon.c_str());
        //解析json串失败, 不处理
        if (document.HasParseError())
        {
            g_LogRecorder.WriteWarnLogEx(__FUNCTION__,
                "***Warning: Parse ST Json Format Failed[%s].", sRespon.c_str());
        }
        else
        {
            //判断json串是否有指定字段
            if (document.HasMember("result") && document["result"].IsString())
            {
                string sResult = document["result"].GetString();
                if (sResult == "error")
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ST Return Get Feature Failed[%s]", sRespon.c_str());
                }
                else if (sResult == "success")
                {
                    if (document.HasMember("success") && document["success"].IsArray() && document["success"].Size() > 0)
                    {
                        for (int i = 0; i < document["success"].Size(); i++)
                        {
                            if (document["success"][i].HasMember("qualityScore") && document["success"][i]["qualityScore"].IsDouble() &&
                                document["success"][i].HasMember("feature") && document["success"][i]["feature"].IsString())
                            {
                                strcpy(pSubMessage->pFeature, document["success"][i]["feature"].GetString());
                                pSubMessage->nQualityScore = document["success"][i]["qualityScore"].GetDouble();
                                bRet = true;
                            }
                            else
                            {
                                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ST Return Get Feature Failed[%s]", sRespon.c_str());
                            }
                        }
                    }
                    else
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ST Return Get Feature Failed[%s]", sRespon.c_str());
                    }
                }
                else
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ST Return Json Wrong[%s].", sRespon.c_str());
                }
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ST  Return Json Wrong[%s].", sRespon.c_str());
            }
        }
    }
    else
    {
        g_LogRecorder.WriteDebugLog(__FUNCTION__, "ST Respon Json Null!");
    }
    
    return bRet;
}
//5. 向zmq代理发送图片及特征值信息, 由分析服务接收
bool CSnapCollectService::SendImageInfo(LPSUBMESSAGE pSubMessage)
{
    strcpy(pSubMessage->pHead, pSubMessage->pDeviceID);
    strcpy(pSubMessage->pOperationType, "add");
    strcpy(pSubMessage->pSource, m_ConfigRead.m_sServerID.c_str());

    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType&allocator = document.GetAllocator();

    string sFaceUUID = GetUUID();
    document.AddMember(JSONFACEUUID,    rapidjson::StringRef(sFaceUUID.c_str()), allocator);
    document.AddMember(JSONFEATURE,     rapidjson::StringRef(pSubMessage->pFeature), allocator);
    document.AddMember(JSONTIME,        pSubMessage->nImageTime, allocator);
    document.AddMember(JSONDRIVE, rapidjson::StringRef("D"), allocator);
    document.AddMember(JSONSERVERIP, rapidjson::StringRef("NULL"), allocator);
    document.AddMember(JSONFACERECT, rapidjson::StringRef("NULL"), allocator);
    document.AddMember(JSONFACEURL, rapidjson::StringRef(pSubMessage->pFaceURL), allocator);
    document.AddMember(JSONBKGURL, rapidjson::StringRef(pSubMessage->pBkgURL), allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    pSubMessage->sPubJsonValue = string(buffer.GetString());

    m_pZeromqManage->PubMessage(pSubMessage);
    return true;
}
LPSUBMESSAGE CSnapCollectService::GetFreeResource()
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
    return pSubMessage;
}
void CSnapCollectService::FreeResource(LPSUBMESSAGE pSubMessage)
{
    EnterMutex();
    pSubMessage->Init();
    m_listSubMessageResource.push_back(pSubMessage);
    LeaveMutex();
}
string CSnapCollectService::GetUUID()
{
    EnterMutex();
    char buffer[64] = { 0 };
    GUID guid;
    CoCreateGuid(&guid);
    _snprintf(buffer, sizeof(buffer),
        "%08X%04X%04x%02X%02X%02X%02X%02X%02X%02X%02X",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2],
        guid.Data4[3], guid.Data4[4], guid.Data4[5],
        guid.Data4[6], guid.Data4[7]);
    LeaveMutex();
    return buffer;
}
void CSnapCollectService::EnterMutex()
{
#ifdef __WINDOWS__
    EnterCriticalSection(&m_cs);
#else
    pthread_mutex_lock(&m_mutex);
#endif
}
void CSnapCollectService::LeaveMutex()
{
#ifdef __WINDOWS__
    LeaveCriticalSection(&m_cs);
#else
    pthread_mutex_unlock(&m_mutex);
#endif
}
unsigned long long CSnapCollectService::ChangeTimeToSecond(string sTime)
{
    tm tTime;
    try
    {
        sscanf_s(sTime.c_str(), "%d-%d-%d %d:%d:%d", &(tTime.tm_year), &(tTime.tm_mon), &(tTime.tm_mday),
            &(tTime.tm_hour), &(tTime.tm_min), &(tTime.tm_sec));
    }
    catch (...)
    {
        return -3;
    }

    tTime.tm_year -= 1900;
    tTime.tm_mon -= 1;
    unsigned long long nBeginTime = (unsigned long long)mktime(&tTime);
    if (nBeginTime == -1)
    {
        return -3;
    }

    return nBeginTime;
}