
#include "tbox/Common.h"
#include "FTP/Upload.h"
#include "SaveFiles/SaveFiles.h"
#include "PublishSignals/PublishSignals.h"

#include <cstdio>
#include <pthread.h>
#include <semaphore.h>

#include "Logger/Logger.h"
#include "tbox/LED_Control.h"

static sem_t gstc_sem_pow; 

static int FillReturnMask(unsigned int PowerMode, unsigned long *pMainMask, unsigned long *pExpMask)
{
   unsigned long ReturnMask = 0x0000;
   switch(PowerMode)
   {
      case LP_MODE_STANDBY:
      case LP_MODE_OFF:
         ReturnMask |= RTU_WKUP_DIN0;
         *pMainMask = ReturnMask;
         XLOG_DEBUG("MAIN CPU Wake Up with MASK: 0x{:X}", *pMainMask);
         break;
      default:
         break;
   }

   return 0;
}

void DINHander(input_int_t InPut) {
	sem_post(&gstc_sem_pow);
	sem_destroy(&gstc_sem_pow);
}

void *enter_stop_mode_thread(void *arg) {
   
   unsigned long  MainCpuMask = 0, ExpBoardMask = 0;
   FillReturnMask(LP_MODE_OFF, &MainCpuMask, &ExpBoardMask);

   sem_wait(&gstc_sem_pow);

   XLOG_INFO("Measurement Stop.");
   // 停止采集
   pre_to_stop_mode_1();
   // 停止发布
   publish_stop();
   save_stop();
   // 保存缓冲区数据，并停止文件保存
   save_files(0, 0, 1);
   // 上传数据文件
	if (-1 == sftp_upload() ) {
       XLOG_ERROR("Upload Failed.");
   } else {
      XLOG_INFO("Upload successfully.");
   }

   // 同步时间，关闭网络
   pre_to_stop_mode_2();
   // 关闭状态指示灯
   DEVICE_STATUS_OFF();
   XLOG_INFO("Enter STOP mode!");

   int ret = RTUEnterStop(MainCpuMask, ExpBoardMask);
   if(NO_ERROR != ret ){
      DEVICE_STATUS_ERROR();
      XLOG_ERROR("ERROR Going to STOP mode: {}.", Str_Error(ret));
   }

   pthread_exit(NULL);
}

// init_type 类型，KL15 是否上电
// 0 -- KL15 未上电
// 1 -- KL15 上电
int power_management_init(int init_type) {
   
   if (init_type == 0) {
      unsigned long  MainCpuMask = 0, ExpBoardMask = 0;
      FillReturnMask(LP_MODE_OFF, &MainCpuMask, &ExpBoardMask);
      int ret = RTUEnterStop(MainCpuMask, ExpBoardMask);
      if(NO_ERROR != ret ){
         DEVICE_STATUS_ERROR();
         XLOG_ERROR("ERROR Going to STOP mode: {}.", Str_Error(ret));
      }
      return 0;
   }

   sem_init(&gstc_sem_pow, 0, 0);
   DIGIO_RemoveInterruptService(DIGITAL_INPUT_0);
	int retval = DIGIO_ConfigureInterruptService(DIGITAL_INPUT_0, FALLING_EDGE, (void(*)(input_int_t))&DINHander, 1);
	if (retval) {
      XLOG_ERROR("DIGIO_ConfigureInterruptService error: {}", Str_Error(retval));
      return -1;
	}
	
	pthread_t tid;
   
   pthread_create(&tid, NULL, enter_stop_mode_thread, NULL);
   pthread_setname_np(tid, "power manager");
   pthread_detach(tid);

   return 0;
}
