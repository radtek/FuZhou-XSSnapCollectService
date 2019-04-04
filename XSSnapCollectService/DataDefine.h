#pragma once
#include "acl_cpp/lib_acl.hpp"
#include "acl/lib_acl.h"
#include <stdint.h>
#include <set>
using namespace std;

#define THREADWAITTIME      10                  //线程等待时间(ms)
#define FACEIMAGELEN        1024 * 1024 * 2     //face图片最大Buf长度
#define MAXFACEIMAGEINFO    150                 //原始资源池最大数
#define HEARTBEATMSG        "heartbeat"         //心跳

//zeromq命令
#define COMMANDADDLIB       "addlib"        //增加重点库
#define COMMANDDELLIB       "dellib"        //删除重点库
#define COMMANDADD          "add"           //增加特征值命令
#define COMMANDDEL          "del"           //删除特征值命令
#define COMMANDCLEAR        "clear"         //清空特征值库命令
#define COMMANDSEARCH       "search"        //搜索特征值命令
#define COMMANDLAYOUTSEARCH "layoutsearch"  //布控服务布控命令
#define LAYOUTLIBADDRESSINFO "layoutlibinfo"//布控分析服务特征值结点指针信息订阅
//Json串字段定义
#define JSONLIBINFO     "libinfo"       //分析服务发布特征值结点信息布控库名
#define JSONLIBINDEX    "index"         //重点库特征值分片索引

#define JSONLAYOUTDATA  "data"          //布控结果数组
#define JSONLAYOUTFACEUUID "layoutfaceuuid" //布控图片FaceUUID
#define JSONLIBID       "libid"         //重点库ID
#define JSONCHECKPOINT  "checkpoint"    //卡口ID
#define JSONFACEUUID    "faceuuid"      //抓拍图片特征值FaceUUID
#define JSONSCORE       "score"         //比对分数
#define JSONFEATURE     "feature"       //特征值
#define JSONFACERECT    "facerect"      //人脸图片坐标
#define JSONFACEURL     "face_url"      //人脸url
#define JSONBKGURL      "bkg_url"       //背景图url
#define JSONTIME        "imagetime"     //抓拍时间
#define JSONDRIVE       "imagedisk"     //图片保存在采集服务上的驱动盘符
#define JSONSERVERIP    "imageip"       //采集服务IP
//Kafka服务发送JSON串定义
#define KAFKADEVICEID   "device_id"     //镜头编码
#define KAFKASNAPTIME   "face_time"     //抓拍时间
#define KAFKAUNIXTIME   "unix_time"     //抓拍时间(unix时间, ms)
#define KAFKAFACEURL    "face_url"      //人脸图片路径
#define KAFKABKGURL     "bkg_url"       //背景图片路径

//Redis Hash表字段定义
#define HASHPICTURE     "Picture"
#define HASHFACE        "Face"
#define HASHTIME        "Time"
#define HASHSCORE       "Score"
#define HASHFEATURE     "Feature"

#define SUBSCRIBEPATTERN    "Checkpoints."          //Redis订阅
#define SQLMAXLEN           1024 * 4                //SQL语句最大长度

#define MAXLEN          128
#define MAXIPLEN        20              //IP, FaceRect最大长度
#define FEATURELEN      4096
#define FEATUREMIXLEN   500             //Feature最短长度, 小于此长度定为不合法
#define THREADNUM       8               //获取特征值处理线程数
#define STTESTLIB       "CollectTemp"   //批量入库服务入分析服务临时库名

//数据库表名
#define LAYOUTCHECKPOINTTABLE   "layoutcheckpoint"  //布控卡口表
#define STORELIBTABLE           "storelib"          //重点库表
#define STOREFACEINFOTABLE      "storefaceinfo"     //重点库图片表
#define LAYOUTRESULTTABLE       "layoutresult"      //布控比对结果表
#define SERVERTYPETABLE         "servertype"        //服务类型表
#define SERVERINFOTABLE         "serverinfo"        //服务信息表
#define RELATIVESERVERTABLE     "relativeserver"    //采集关联镜头表
#define IPCAMERATABLE           "ipcamera"          //镜头表

#define FEATUREMAXLEN   1024 * 4
#define MAXTHREADCOUNT  128

enum ErrorCode      //错误码定义
{
    INVALIDERROR = 0,       //无错误
    ServerNotInit = 12001,   //服务尚未初始化完成
    DBAleadyExist,          //库己存在
    DBNotExist,             //库不存在
    FaceUUIDAleadyExist,    //FaceUUID己存在
    FaceUUIDNotExist,       //FaceUUID不存在
    ParamIllegal,           //参数非法
    NewFailed,              //new申请内存失败
    JsonFormatError,        //json串格式错误
    CommandNotFound,        //不支持的命令
    HttpMsgUpperLimit,      //http消息待处理数量己达上限
    PthreadMutexInitFailed, //临界区初始化失败
    FeatureNumOverMax,      //批量增加特征值数量超标
    JsonParamIllegal,       //Json串有值非法
    MysqlQueryFailed,       //Mysql操作执行失败.
    VerifyFailed,           //比对失败
    PushFeatureToAnalyseFailed, //特征值推送给分析服务失败
    STSDKInitFailed,        //ST库初始化失败
    STJsonFormatFailed,     //ST返回结果Json串解析失败
    GetSTFeatureFailed,      //创建ST重点库失败
    AddImageToSTFailed,     //推送图片给ST服务器获取特征值, 返回error失败
    STRepGetFeatureFailed,  //ST服务器获取单张图片特征值失败
    STRepFileNameNotExist,  //入库ST服务器, 返回文件名不存在
    GetLocalPathFailed,     //获取服务本地路径失败
    LoadLibFailed,          //加载库失败
    GetProcAddressFailed,   //获取函数地址失败
    FRSEngineInitFailed,    //本地特征值获取库初始化失败
    ConvertBMPFailed,       //图片转BMP失败
    GetLocalFeatureFailed,  //本地获取图片特征值失败
    NotEnoughResource,      //无可用资源, 稍候上传
    GetPictureFailed,       //给定图片url地址无法获取到图片
    TaskIDNotExist          //任务ID不存在
};


//任务类型
enum TASKTYPE
{
    INVALIDTASK,
    CHECKPOINTADDFEATURE,   //向卡口增加特征值
    CHECKPOINTDELFEATURE,   //从卡口删除特征值
    CHECKPOINTCLEARFEATURE, //从卡口删除特征值
    CHECKPOINTSEARCH,       //按时间, 卡口搜索特征值
};

typedef struct _SubMessage
{
    char pHead[MAXLEN];                 //订阅消息头
    char pOperationType[MAXLEN];        //订阅消息操作类型
    char pSource[MAXLEN];               //订阅消息源
    string sSubJsonValue; //订阅消息Json串
    string sPubJsonValue;     //发布消息Json串
         
    char pDeviceID[MAXLEN];     //卡口ID
    char pFaceUUID[MAXLEN];     //特征值FaceUUID
    int64_t nImageTime;         //图片抓拍时间
    int nQualityScore;          //ST返回图片质量分数
    char pFeature[FEATURELEN];  //特征值
    char pDisk[2];              //图片保存磁盘
    char pImageIP[MAXIPLEN];    //图片保存服务器IP
    char pFaceRect[MAXIPLEN];   //人脸坐标
    char pFaceURL[512];        //图片人脸URL
    char pBkgURL[512];         //图片背景URL
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
