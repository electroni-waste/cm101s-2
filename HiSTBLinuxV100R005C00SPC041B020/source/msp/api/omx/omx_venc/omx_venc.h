/*========================================================================
Open MAX   Component: Video Encoder
This module contains the class definition for openMAX decoder component.
==========================================================================*/
#ifndef __OMX_VENC_H__
#define __OMX_VENC_H__

//khrons standard head file
#include "OMX_Component.h"
#include "OMX_Video.h"
//#include "OMX_Custom.h"


#include "OMX_Types.h"

#include "hi_drv_venc.h"

#include "omx_allocator.h"
#include "omx_venc_queue.h"
#include "omx_venc_drv.h"

//specific-version NO.1.1
#define KHRONOS_1_1                                  //??
#define OMX_SPEC_VERSION			0x00000101       //版本号宏定义

#define MAX_PORT_NUM				2

#define MAX_BUF_NUM_INPUT			6//30
#define MAX_BUF_NUM_OUTPUT			20//30

#define MAX_FRAME_WIDTH				1920
#define MAX_FRAME_HEIGHT			1088

#define DEFAULT_FPS				    30
#define DEFAULT_BITRATE             (5*1024*1024)
#define DEFAULT_ALIGN_SIZE			4096                      //?? 传说中的4K边界边界?

#define DEF_MAX_IN_BUF_CNT			1  //3
#define DEF_MIN_IN_BUF_CNT			1

#define DEF_MAX_OUT_BUF_CNT		    2
#define DEF_MIN_OUT_BUF_CNT		    1   //6

#define DEF_OUT_BUFFER_SIZE         (1000000)

#define ENABLE_BUFFER_LOCK
/**
* A pointer to this struct is passed to the OMX_SetParameter when the extension
* index for the 'OMX.google.android.index.enableAndroidNativeBuffers' extension
* is given.
* The corresponding extension Index is OMX_GoogleIndexUseAndroidNativeBuffer.
* This will be used to inform OMX about the presence of gralloc pointers
instead
* of virtual pointers
*/

typedef struct StoreMetaDataInBuffersParams
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bStoreMetaData;
} StoreMetaDataInBuffersParams;

struct PrependSPSPPSToIDRFramesParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnable;
};

typedef struct AutoRequestIFrmParams
{
    OMX_U32 nSize;
    OMX_U32 nRes;
    OMX_BOOL bEnable;
} AutoRequestIFrmParams;
////////////////////////////////////////////////////////////////////////// add end
#define BITS_PER_LONG				32

//#define ESHUTDOWN					108
//#define ERESTARTSYS					512

/*设置输入参数中的 版本号和结构大小*/
#define CONFIG_VERSION_SIZE(pParam,Type)  do{\
        pParam->nVersion.nVersion = OMX_SPEC_VERSION;\
        pParam->nSize             = sizeof(Type);\
    }while(0)


#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))                  //算出数组中含有的元素个数


#define OMX_CHECK_ARG_RETURN(a)  do{if ((a)){DEBUG_PRINT_ERROR("[%s],line: %d", __func__,__LINE__);return OMX_ErrorBadParameter;}}while(0)

#define ALIGN_UP(val, align) ( (val+((align)-1))&~((align)-1) )

#define FRAME_SIZE(w , h)  (((w) * (h) * 2))  //(((w) * (h) * 3) / 2)

// state transactions pending bits
#if 0
enum                                     /*在component私有结构中，作为各种处理的标志，处理完该事件后，把该bit位置回0*/
{
    OMX_STATE_IDLE_PENDING				= 0x1,               //idle(空闲)等待标志,该标志为1时，在相应事件处理函数中处理该等待的操作，处理完后把该位 置回0
    OMX_STATE_LOADING_PENDING			= 0x2,
    OMX_STATE_INPUT_ENABLE_PENDING		= 0x3,
    OMX_STATE_OUTPUT_ENABLE_PENDING		= 0x4,
    OMX_STATE_OUTPUT_DISABLE_PENDING	= 0x5,
    OMX_STATE_INPUT_DISABLE_PENDING		= 0x6,
    OMX_STATE_OUTPUT_FLUSH_PENDING		= 0x7,
    OMX_STATE_INPUT_FLUSH_PENDING		= 0x8,
    OMX_STATE_PAUSE_PENDING				= 0x9,
    OMX_STATE_EXECUTE_PENDING			= 0xA
};
#endif

// Deferred callback identifiers
/*enum {                                                        //用于表示各个 CMD 类型,以便在插入队列的时候判断要把消息的处理是要插入哪个队列
	OMX_GENERATE_COMMAND_DONE			= 0x1,
	OMX_GENERATE_FTB					= 0x2,
	OMX_GENERATE_ETB					= 0x3,
	OMX_GENERATE_COMMAND				= 0x4,
	OMX_GENERATE_EBD					= 0x5,
	OMX_GENERATE_FBD					= 0x6,
	OMX_GENERATE_FLUSH_INPUT_DONE		= 0x7,
	OMX_GENERATE_FLUSH_OUTPUT_DONE		= 0x8,
	OMX_GENERATE_START_DONE				= 0x9,
	OMX_GENERATE_PAUSE_DONE				= 0xA,
	OMX_GENERATE_RESUME_DONE			= 0xB,
	OMX_GENERATE_STOP_DONE				= 0xC,
	OMX_GENERATE_EOS_DONE				= 0xD,
	OMX_GENERATE_HARDWARE_ERROR			= 0xE,
	OMX_GENERATE_IMAGE_SIZE_CHANGE		= 0xF,
	OMX_GENERATE_CROP_RECT_CHANGE		= 0x10,
	OMX_GENERATE_UNUSED					= 0x11
};*/

enum
{
    INPUT_PORT_INDEX	= 0,
    OUTPUT_PORT_INDEX	= 1
};

struct port_property                                           //作为端口私有结构的成员变量之一,描述了端口特性
{
    OMX_U32 port_dir;                                     //端口的方向:in /out
    OMX_U32 min_count;                                    //对应于标准OMX_PARAM_PORTDEFINITIONTYPE类型定义中的nBufferCountMin变量，标识The minimum number of buffers this port requires
    OMX_U32 max_count;                                    //对应于标准OMX_PARAM_PORTDEFINITIONTYPE类型定义中的nBufferCountActual变量，标识The actual number of buffers allocated on this port
    OMX_U32 buffer_size;                                  //对应于标准OMX_PARAM_PORTDEFINITIONTYPE类型定义中的nBufferSize，标识Size, in bytes, for buffers to be used for this channel
    OMX_U32 alignment;                                    //用于申请内存之类的动作!!4K对齐
};

/*struct omx_hisi_extern_index
{
	OMX_S8 index_name[OMX_MAX_STRINGNAME_SIZE];
	OMX_HISI_EXTERN_INDEXTYPE index_type;
};*/

struct codec_info                                              //编码器协议类型结构体,具体包含:
{
    const OMX_STRING role_name;                                     //编码类型名称(uchar型数组)
    OMX_VIDEO_CODINGTYPE compress_fmt;                         //OMX标准头文件omx_video.h中定义的视频协议枚举类型,最终赋值给OMX_COMPONENT_PRIVATE类型的m_dec_fmt，OMX_VIDEO_PARAM_PORTFORMATTYPE或OMX_VIDEO_PORTDEFINITIONTYPE类型中的eCompressionFormat
    //enum venc_codec_type codec_type;                           //OMX标准头文件omx_video.h中定义的视频协议枚举类型,最终赋值给OMX_COMPONENT_PRIVATE类型的drv_ctx.venc_chan_attr[VENC_MAX_CHN].chan_cfg.protocol,具体定义在hisi_venc.h，应与原来hi_unf_common.h中定义一致
    HI_UNF_VCODEC_TYPE_E codec_type;
};

struct codec_profile_level                                   //编码不需要� 暂时保留
{
    OMX_S32 profile;
    OMX_S32 level;
};/**/


struct frame_info                                            //已包含在编码通道属性中
{
    OMX_U32 frame_width;
    OMX_U32 frame_height;
    OMX_U32 stride;
    //OMX_U32 scan_lines;
    //OMX_U32 crop_top;
    //OMX_U32 crop_left;
    //OMX_U32 crop_width;
    //OMX_U32 crop_height;
};/**/

// port private for omx
typedef struct OMX_PORT_PRIVATE                                 //端口的私有结构体，作为组件的私有结构体的成员之一，在OMX_COMPONENT_PRIVATE类型中，对应输入/输出端口各有一套
{
    OMX_BUFFERHEADERTYPE** m_omx_bufhead;                       //对应OMX标准定义，用于存放该端口要处理的buffer
    //struct port_property port_pro;                              //端口特性结构体，其中包含的内容多在标准的端口类型OMX_PARAM_PORTDEFINITIONTYPE中对应

    OMX_PARAM_PORTDEFINITIONTYPE port_def;
    //OMX_U32	m_port_index;                                       //端口ID索引号   在OMX_PARAM_PORTDEFINITIONTYPE那里有定义；

    OMX_U32 m_buf_cnt;                                          //相当于标识端口第i个buffer有无数据的flag，m_buf_cnt的每一位为1，则该位对应的序号的buffer有数据，可用于判断port buffer 的空满，找可写入的空闲buffer等操作

    OMX_U32 m_usage_bitmap;                                     //与上述m_buf_cnt，用法相似，只是它用于 mark buffer to be allocated(标识自己申请到的buffer，方便以后做free操作)
    OMX_U32	m_buf_pend_cnt;                                     //记录待用户处理的buffer数目(实际上组件内部已经处理完成，只是等待告知用户)~~如:fill_this_buffer_porxy时++,在fill_this_buffer_done时--.

    //OMX_U32	m_port_enabled;                                     //端口使能标志位，也对应于端口标准定义OMX_PARAM_PORTDEFINITIONTYPE中的bEnabled位
    //OMX_U32	m_port_populated;                                   //端口充满(?)标志位，也对应于端口标准定义OMX_PARAM_PORTDEFINITIONTYPE中的bPopulated位，具体描述见bPopulated的注释【表示端口所申请的buffer数目已经达到最大】

    OMX_BOOL m_port_reconfig;                                   //端口重新配置标志位，默认为false，当处理OMX_GENERATE_IMAGE_SIZE_CHANGE命令事件时，把该标识置1，此时不能进行fill/empty_this_buffer等操作，等到接到COMMON_DONE及OMX_CommandPortEnable的事件处理时才会把它置回0
    OMX_BOOL m_port_flushing;                                   //端口buffer刷新标志位，默认为false，当收到OMX_CommandFlush命令时调用handle_command_flush函数处理，把该标识置1，此时不能进行fill/empty_this_buffer等操作，等到接到OMX_GENERATE_FLUSH_INPUT_DONE的事件处理时才会把它置回0


    //struct frame_info pic_info;               //在venc_driver_context数据类型中有定义编码宽度高度信息，可否复用?   (目前相当于输入的帧信息)

    venc_user_info** m_venc_bufhead;                 //??用于对标准m_omx_bufhead的补充buffer??

} OMX_PORT_PRIVATE;

//component private for omx                                     //组件的私有结构体，实际应用绝大部分都是这个结构体，其中包含标准定义中的OMX_COMPONENTTYPE类型数据指针
typedef struct OMX_COMPONENT_PRIVATE
{
    pthread_mutex_t m_lock;                                     //互斥量，在对共享资源的读写操作时，一定要用互斥量加以保护，这里的共享资源主要是:三个队列，管道读写端
    OMX_COMPONENTTYPE* m_pcomp;                                 //组件结构体
    OMX_STATETYPE m_state;                                      //组件状态
    OMX_U32 m_flags;                                            //每一位对应了flags_bit_positions中定义的事件
    OMX_VIDEO_CODINGTYPE m_encoder_fmt;                         //与codec_info对应
    OMX_S8 m_role[OMX_MAX_STRINGNAME_SIZE];                     //编码类型名字标识
    OMX_S8 m_comp_name[OMX_MAX_STRINGNAME_SIZE];                //编码器名字指针
    OMX_PTR m_app_data;                                         //由外部应用程序APP传进来的一个参数，用于给APP区分不同的component，在应用程序调用OMX_GetHandle时与回调函数结构体指针一起传入
    OMX_CALLBACKTYPE m_cb;                                      //由外部应用程序APP传进来回调函数指针，在应用程序调用OMX_GetHandle时与上述m_app_data回调函数结构体指针一起传入

    OMX_TICKS m_pre_timestamp;                                  //pts 时间戳

    OMX_PORT_PRIVATE m_port[MAX_PORT_NUM];                      //port的私有结构
    OMX_BOOL m_use_native_buf;                                  //用自身buffer标识，在get/set_parameter的 OMX_GoogleIndexGetAndroidNativeBufferUsage  判断分支中应用

    OMX_BOOL m_store_metadata_buf;                              //用自身buffer标识，在get/set_parameter的 OMX_GoogleIndexStoreMetaDataInBuffers  判断分支中应用
    OMX_BOOL m_prepend_sps_pps;                                 //用于标示当前是否支持在每个I帧前都发SPS/PPS数据
    pthread_t msg_thread_id;                                    //消息线程     ->write
    pthread_t event_thread_id;                                  //事件处理线程 ->read

    volatile OMX_BOOL event_thread_exit;                                 //事件处理线程退出标志
    volatile OMX_BOOL msg_thread_exit;                                   ////消息线程退出标志


    venc_drv_context drv_ctx;         //作为Component的私有结构，定于在omx_venc_drv.h中包含编码器设备文件标识符号、编码器通道属性
    OMX_S32 m_pipe_in;                              //read this pipe
    OMX_S32 m_pipe_out;                             //write this pipe

    sem_t m_async_sem;                          //异步信号量 ，用于读取通道时加信号量保护
    sem_t m_cmd_lock;                           //命令信号量 (相当于锁) ??
    #ifdef ENABLE_BUFFER_LOCK
    sem_t m_buf_lock;
    #endif
    omx_event_queue m_ftb_q;                    //输出数据队列           /*注意:每次写完队列之后对都会对管道进行相应的写，在事件处理线程中对管道进行读操作，读出消息处理完后便出丢列*/
    omx_event_queue m_cmd_q;                    //命令队列
    omx_event_queue m_etb_q;                    //输入源队列
    void  *bufferaddr_RGB;     //虚拟地址
    HI_U32 bufferaddr_Phy_RGB;
	HI_U32 buffer_size_RGB;    //buffer alloc size

    HI_HANDLE hTDE;
    //allocator_handle_t allocator_handle;        //分配内存，用到IOH设备时，用于记录ION句柄~
} OMX_COMPONENT_PRIVATE;

/*==========================================================================*/
// bit operation functions
static inline void bit_set(OMX_U32* addr, OMX_U32 nr)                          // 对数据的某一位设置为 1 (该函数对64位同样有效)
{
    addr[nr / BITS_PER_LONG] |= (1 << (nr % BITS_PER_LONG));
}

static inline void bit_clear(OMX_U32* addr, OMX_U32 nr)                         // 对数据的某一位设置为清0 (该函数对64位同样有效)
{
    addr[nr / BITS_PER_LONG] &= ~((OMX_U32)(1 << (nr % BITS_PER_LONG)));
}

static inline OMX_S32 bit_present(const OMX_U32* addr, OMX_U32 nr)         // 判断数据某一位是否为 1;如果为1,则返回1，否则返回0 (该函数对64位同样有效)
{
    return ((1 << (nr % BITS_PER_LONG)) &
            (((OMX_U32*)addr)[nr / BITS_PER_LONG])) != 0;
}

static inline OMX_S32 bit_absent(const OMX_U32* addr, OMX_U32 nr)          // 判断数据某一位是否为 0；如果为0,则返回1，否则返回0
{
    return !bit_present(addr, nr);
}

#endif // __OMX_VENC_H__
