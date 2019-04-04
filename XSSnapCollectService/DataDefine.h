#pragma once
#include "acl_cpp/lib_acl.hpp"
#include "acl/lib_acl.h"
#include <stdint.h>
#include <set>
using namespace std;

#define THREADWAITTIME      10                  //�̵߳ȴ�ʱ��(ms)
#define FACEIMAGELEN        1024 * 1024 * 2     //faceͼƬ���Buf����
#define MAXFACEIMAGEINFO    150                 //ԭʼ��Դ�������
#define HEARTBEATMSG        "heartbeat"         //����

//zeromq����
#define COMMANDADDLIB       "addlib"        //�����ص��
#define COMMANDDELLIB       "dellib"        //ɾ���ص��
#define COMMANDADD          "add"           //��������ֵ����
#define COMMANDDEL          "del"           //ɾ������ֵ����
#define COMMANDCLEAR        "clear"         //�������ֵ������
#define COMMANDSEARCH       "search"        //��������ֵ����
#define COMMANDLAYOUTSEARCH "layoutsearch"  //���ط��񲼿�����
#define LAYOUTLIBADDRESSINFO "layoutlibinfo"//���ط�����������ֵ���ָ����Ϣ����
//Json���ֶζ���
#define JSONLIBINFO     "libinfo"       //�������񷢲�����ֵ�����Ϣ���ؿ���
#define JSONLIBINDEX    "index"         //�ص������ֵ��Ƭ����

#define JSONLAYOUTDATA  "data"          //���ؽ������
#define JSONLAYOUTFACEUUID "layoutfaceuuid" //����ͼƬFaceUUID
#define JSONLIBID       "libid"         //�ص��ID
#define JSONCHECKPOINT  "checkpoint"    //����ID
#define JSONFACEUUID    "faceuuid"      //ץ��ͼƬ����ֵFaceUUID
#define JSONSCORE       "score"         //�ȶԷ���
#define JSONFEATURE     "feature"       //����ֵ
#define JSONFACERECT    "facerect"      //����ͼƬ����
#define JSONFACEURL     "face_url"      //����url
#define JSONBKGURL      "bkg_url"       //����ͼurl
#define JSONTIME        "imagetime"     //ץ��ʱ��
#define JSONDRIVE       "imagedisk"     //ͼƬ�����ڲɼ������ϵ������̷�
#define JSONSERVERIP    "imageip"       //�ɼ�����IP
//Kafka������JSON������
#define KAFKADEVICEID   "device_id"     //��ͷ����
#define KAFKASNAPTIME   "face_time"     //ץ��ʱ��
#define KAFKAUNIXTIME   "unix_time"     //ץ��ʱ��(unixʱ��, ms)
#define KAFKAFACEURL    "face_url"      //����ͼƬ·��
#define KAFKABKGURL     "bkg_url"       //����ͼƬ·��

//Redis Hash���ֶζ���
#define HASHPICTURE     "Picture"
#define HASHFACE        "Face"
#define HASHTIME        "Time"
#define HASHSCORE       "Score"
#define HASHFEATURE     "Feature"

#define SUBSCRIBEPATTERN    "Checkpoints."          //Redis����
#define SQLMAXLEN           1024 * 4                //SQL�����󳤶�

#define MAXLEN          128
#define MAXIPLEN        20              //IP, FaceRect��󳤶�
#define FEATURELEN      4096
#define FEATUREMIXLEN   500             //Feature��̳���, С�ڴ˳��ȶ�Ϊ���Ϸ�
#define THREADNUM       8               //��ȡ����ֵ�����߳���
#define STTESTLIB       "CollectTemp"   //���������������������ʱ����

//���ݿ����
#define LAYOUTCHECKPOINTTABLE   "layoutcheckpoint"  //���ؿ��ڱ�
#define STORELIBTABLE           "storelib"          //�ص���
#define STOREFACEINFOTABLE      "storefaceinfo"     //�ص��ͼƬ��
#define LAYOUTRESULTTABLE       "layoutresult"      //���رȶԽ����
#define SERVERTYPETABLE         "servertype"        //�������ͱ�
#define SERVERINFOTABLE         "serverinfo"        //������Ϣ��
#define RELATIVESERVERTABLE     "relativeserver"    //�ɼ�������ͷ��
#define IPCAMERATABLE           "ipcamera"          //��ͷ��

#define FEATUREMAXLEN   1024 * 4
#define MAXTHREADCOUNT  128

enum ErrorCode      //�����붨��
{
    INVALIDERROR = 0,       //�޴���
    ServerNotInit = 12001,   //������δ��ʼ�����
    DBAleadyExist,          //�⼺����
    DBNotExist,             //�ⲻ����
    FaceUUIDAleadyExist,    //FaceUUID������
    FaceUUIDNotExist,       //FaceUUID������
    ParamIllegal,           //�����Ƿ�
    NewFailed,              //new�����ڴ�ʧ��
    JsonFormatError,        //json����ʽ����
    CommandNotFound,        //��֧�ֵ�����
    HttpMsgUpperLimit,      //http��Ϣ������������������
    PthreadMutexInitFailed, //�ٽ�����ʼ��ʧ��
    FeatureNumOverMax,      //������������ֵ��������
    JsonParamIllegal,       //Json����ֵ�Ƿ�
    MysqlQueryFailed,       //Mysql����ִ��ʧ��.
    VerifyFailed,           //�ȶ�ʧ��
    PushFeatureToAnalyseFailed, //����ֵ���͸���������ʧ��
    STSDKInitFailed,        //ST���ʼ��ʧ��
    STJsonFormatFailed,     //ST���ؽ��Json������ʧ��
    GetSTFeatureFailed,      //����ST�ص��ʧ��
    AddImageToSTFailed,     //����ͼƬ��ST��������ȡ����ֵ, ����errorʧ��
    STRepGetFeatureFailed,  //ST��������ȡ����ͼƬ����ֵʧ��
    STRepFileNameNotExist,  //���ST������, �����ļ���������
    GetLocalPathFailed,     //��ȡ���񱾵�·��ʧ��
    LoadLibFailed,          //���ؿ�ʧ��
    GetProcAddressFailed,   //��ȡ������ַʧ��
    FRSEngineInitFailed,    //��������ֵ��ȡ���ʼ��ʧ��
    ConvertBMPFailed,       //ͼƬתBMPʧ��
    GetLocalFeatureFailed,  //���ػ�ȡͼƬ����ֵʧ��
    NotEnoughResource,      //�޿�����Դ, �Ժ��ϴ�
    GetPictureFailed,       //����ͼƬurl��ַ�޷���ȡ��ͼƬ
    TaskIDNotExist          //����ID������
};


//��������
enum TASKTYPE
{
    INVALIDTASK,
    CHECKPOINTADDFEATURE,   //�򿨿���������ֵ
    CHECKPOINTDELFEATURE,   //�ӿ���ɾ������ֵ
    CHECKPOINTCLEARFEATURE, //�ӿ���ɾ������ֵ
    CHECKPOINTSEARCH,       //��ʱ��, ������������ֵ
};

typedef struct _SubMessage
{
    char pHead[MAXLEN];                 //������Ϣͷ
    char pOperationType[MAXLEN];        //������Ϣ��������
    char pSource[MAXLEN];               //������ϢԴ
    string sSubJsonValue; //������ϢJson��
    string sPubJsonValue;     //������ϢJson��
         
    char pDeviceID[MAXLEN];     //����ID
    char pFaceUUID[MAXLEN];     //����ֵFaceUUID
    int64_t nImageTime;         //ͼƬץ��ʱ��
    int nQualityScore;          //ST����ͼƬ��������
    char pFeature[FEATURELEN];  //����ֵ
    char pDisk[2];              //ͼƬ�������
    char pImageIP[MAXIPLEN];    //ͼƬ���������IP
    char pFaceRect[MAXIPLEN];   //��������
    char pFaceURL[512];        //ͼƬ����URL
    char pBkgURL[512];         //ͼƬ����URL
    _SubMessage()
    {
        memset(pHead, 0, sizeof(pHead));
        memset(pOperationType, 0, sizeof(pOperationType));
        memset(pSource, 0, sizeof(pSource));
        sSubJsonValue = "";
        sPubJsonValue = "";

        memset(pDeviceID, 0, sizeof(pDeviceID));
        memset(pFaceUUID, 0, sizeof(pFaceUUID));
        nImageTime = 0;
        nQualityScore = 0;
        memset(pFeature, 0, sizeof(pFeature));
        memset(pDisk, 0, sizeof(pDisk));
        memset(pImageIP, 0, sizeof(pImageIP));
        memset(pFaceRect, 0, sizeof(pFaceRect));
        memset(pFaceURL, 0, sizeof(pFaceURL));
        memset(pBkgURL, 0, sizeof(pBkgURL));
    }
    void Init()
    {
        memset(pHead, 0, sizeof(pHead));
        memset(pOperationType, 0, sizeof(pOperationType));
        memset(pSource, 0, sizeof(pSource));
        sSubJsonValue = "";
        sPubJsonValue = "";

        memset(pDeviceID, 0, sizeof(pDeviceID));
        memset(pFaceUUID, 0, sizeof(pFaceUUID));
        nImageTime = 0;
        nQualityScore = 0;
        memset(pFeature, 0, sizeof(pFeature));
        memset(pDisk, 0, sizeof(pDisk));
        memset(pImageIP, 0, sizeof(pImageIP));
        memset(pFaceRect, 0, sizeof(pFaceRect));
        memset(pFaceURL, 0, sizeof(pFaceURL));
        memset(pBkgURL, 0, sizeof(pBkgURL));
    }
}SUBMESSAGE, *LPSUBMESSAGE;
typedef std::list<LPSUBMESSAGE> LISTSUBMESSAGE;
typedef void(*LPSubMessageCallback)(LPSUBMESSAGE pSubMessage, void * pUser);
