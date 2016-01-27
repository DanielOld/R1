/******************** (C) COPYRIGHT 2011 STMicroelectronics ********************
* File Name          : main.c
* Author             : MCD Application Team
* Version            : V3.3.0
* Date               : 21-March-2011
* Description        : Virtual Com Port Demo main file
********************************************************************************
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/

#include "LSM6DS3_ACC_GYRO_driver.h"  
#include "twi_master.h"
#include "nrf_delay.h"



/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Extern variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

//extern BT_Status_t BT_Module_Send (char *Buffer);
//extern BT_Status_t BT_Module_Receive (char *Buffer);



#define SENSITIVITY_8G    0.244/1000 /* G/LSB */

#define SENSITIVITY_2000DPS    70.0/1000 /* dps/LSB */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
LSM6DS3_ACC_GYRO_GDA_t value_G;
Type3Axis16bit_U AngularRate;
float AngularRate_dps[3];

LSM6DS3_ACC_GYRO_XLDA_t value_XL;
Type3Axis16bit_U Acceleration;
float Acceleration_G[3];

Type1Axis16bit_U StepCount;

status_t response;  

/* Extern variables ----------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
/*
 * This part is needed in case we need to bypass the device ROM 
 */

/* Init the Gyroscope */
static void init_LSM6DS3_GYRO(void)
{
  /* Gyro ODR and full scale */
  response = LSM6DS3_ACC_GYRO_W_ODR_G(LSM6DS3_ACC_GYRO_ODR_G_104Hz);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  response = LSM6DS3_ACC_GYRO_W_FS_G(LSM6DS3_ACC_GYRO_FS_G_2000dps);
  if(response==MEMS_ERROR) while(1); //manage here comunication error
}

/* Init the Accelerometer */
static void init_LSM6DS3_ACC(void)
{
  /* Set ACC ODR  */
  response = LSM6DS3_ACC_GYRO_W_ODR_XL(LSM6DS3_ACC_GYRO_ODR_XL_104Hz);
  if(response==MEMS_ERROR) while(1); //manage here comunication error
  
  /* Set ACC full scale */
  response = LSM6DS3_ACC_GYRO_W_FS_XL(LSM6DS3_ACC_GYRO_FS_XL_2g);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  /* BDU Enable */
  response = LSM6DS3_ACC_GYRO_W_BDU(LSM6DS3_ACC_GYRO_BDU_BLOCK_UPDATE);
  if(response==MEMS_ERROR) while(1); //manage here comunication error
}

/* Test Acquisition of sensor samples */
static void Loop_Test_Sample_Aquisition(void)
{
  /* configure device */
  init_LSM6DS3_GYRO();
  init_LSM6DS3_ACC();

  /*
   * Read samples in polling mode (no int)
   */
  while(1)
  {
    /*
     * Read ACC output only if new ACC value is available
     */
    response =  LSM6DS3_ACC_GYRO_R_XLDA(&value_XL);
    if(response==MEMS_ERROR) while(1); //manage here comunication error  
    
    if (LSM6DS3_ACC_GYRO_XLDA_DATA_AVAIL==value_XL)
    {
      LSM6DS3_ACC_GYRO_Get_GetAccData(Acceleration.u8bit);

      /* Transorm LSB into G */
      Acceleration_G[0]=Acceleration.i16bit[0]*SENSITIVITY_8G;
      Acceleration_G[1]=Acceleration.i16bit[1]*SENSITIVITY_8G;
      Acceleration_G[2]=Acceleration.i16bit[2]*SENSITIVITY_8G;
    }

    /* 
     * Read GYRO output only if new gyro value is available
     */
    response =  LSM6DS3_ACC_GYRO_R_GDA(&value_G);
    if(response==MEMS_ERROR) while(1); //manage here comunication error  
    
    if (LSM6DS3_ACC_GYRO_GDA_DATA_AVAIL==value_G)
    {
      LSM6DS3_ACC_GYRO_Get_GetGyroData(AngularRate.u8bit);

      /* Transorm LSB into dps */
      AngularRate_dps[0]=AngularRate.i16bit[0]*SENSITIVITY_2000DPS;
      AngularRate_dps[1]=AngularRate.i16bit[1]*SENSITIVITY_2000DPS;
      AngularRate_dps[2]=AngularRate.i16bit[2]*SENSITIVITY_2000DPS;
    }
  }
}


/* Init the Pedometer */
static void init_LSM6DS3_Pedometer(void)
{
  /* Set ACC ODR  */
  response = LSM6DS3_ACC_GYRO_W_ODR_XL(LSM6DS3_ACC_GYRO_ODR_XL_26Hz);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  /* Enable Pedometer  */
  response = LSM6DS3_ACC_GYRO_W_PEDO_EN(LSM6DS3_ACC_GYRO_PEDO_EN_ENABLED);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  /* Pedometer on INT1 */
  response = LSM6DS3_ACC_GYRO_W_PEDO_STEP_on_INT1(LSM6DS3_ACC_GYRO_INT1_PEDO_ENABLED);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  /* Clear INT1 routing */
  LSM6DS3_ACC_GYRO_WriteReg(LSM6DS3_ACC_GYRO_MD1_CFG, 0x0);

  /* Enable Embedded Functions */
  LSM6DS3_ACC_GYRO_W_FUNC_EN(LSM6DS3_ACC_GYRO_FUNC_EN_ENABLED);
}

/* Test pedometer */
static void Loop_Test_Pedometer(void)
{
  int led_flash;
  int16_t stepcounter;
  
  LSM6DS3_ACC_GYRO_PEDO_EV_STATUS_t PedoStatus;

  /* configure pedometer */
  init_LSM6DS3_Pedometer();

  /* configure pedometer acceleration threshold */
  LSM6DS3_ACC_GYRO_W_PedoThreshold(0x11);

  /* Clear the step counter */
  LSM6DS3_ACC_GYRO_W_PedoStepReset(LSM6DS3_ACC_GYRO_PEDO_RST_STEP_ENABLED);

  /*
   * Handle the event using polling mode
   */
  while(1) {
    LSM6DS3_ACC_GYRO_R_PEDO_EV_STATUS(&PedoStatus);
    if (PedoStatus == LSM6DS3_ACC_GYRO_PEDO_EV_STATUS_DETECTED) {
      if(led_flash) {
        //EKSTM32_LEDOn(LED2);
        led_flash=0;
      }
      else {
        //EKSTM32_LEDOff(LED2);
        led_flash=1;
      }
      LSM6DS3_ACC_GYRO_Get_GetStepCounter(StepCount.u8bit);
      stepcounter=*((int16_t*)StepCount.u8bit);
    }
  }
}

/* Init the Wakeup */
static void init_LSM6DS3_Wakeup(void)
{
  /* Set ACC ODR  */
  response = LSM6DS3_ACC_GYRO_W_ODR_XL(LSM6DS3_ACC_GYRO_ODR_XL_104Hz);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  /* Set Wakeup threshold  */
  response = LSM6DS3_ACC_GYRO_W_WK_THS(2);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  /* Set Wakeup duration  */
  response = LSM6DS3_ACC_GYRO_W_SLEEP_DUR(0);
  if(response==MEMS_ERROR) while(1); //manage here comunication error

  /* Wakeup on INT1 */
  response = LSM6DS3_ACC_GYRO_W_WUEvOnInt1(LSM6DS3_ACC_GYRO_INT1_WU_ENABLED);
  if(response==MEMS_ERROR) while(1); //manage here comunication error
}

/* Test Wakeup */
static void Loop_Test_Wakeup(void)
{
  LSM6DS3_ACC_GYRO_WU_EV_STATUS_t WakeupStatus;

  /* configure pedometer */
  init_LSM6DS3_Wakeup();
  
  /*
   * Handle the event using polling mode
   */
  while(1) {
    LSM6DS3_ACC_GYRO_R_WU_EV_STATUS(&WakeupStatus);
    if (WakeupStatus == LSM6DS3_ACC_GYRO_WU_EV_STATUS_DETECTED) {
      /* handle event */
      WakeupStatus = LSM6DS3_ACC_GYRO_WU_EV_STATUS_NOT_DETECTED;
    }
  }
}


int sensor_init(void)
{
  uint8_t WHO_AM_I = 0x0;

  /* for lsm6ds3 pedometer */
  twi_master_init();
  nrf_delay_ms(5);

  /* Read WHO_AM_I  and check if device is really the LSM6DS3 */
  //I2C_BufferRead(&WHO_AM_I, 0xd6, 0x0f, 1);
  LSM6DS3_ACC_GYRO_R_WHO_AM_I(&WHO_AM_I);
  if (WHO_AM_I != LSM6DS3_ACC_GYRO_WHO_AM_I) while(1); //manage here comunication error

  /* Soft Reset the LSM6DS3 device */
  LSM6DS3_ACC_GYRO_W_SW_RESET(LSM6DS3_ACC_GYRO_SW_RESET_RESET_DEVICE);
  nrf_delay_ms(10);

  init_LSM6DS3_ACC();

#if 0
  /* Soft Reset the LSM6DS3 device */
  //LSM6DS3_ACC_GYRO_W_SW_RESET(LSM6DS3_ACC_GYRO_SW_RESET_RESET_DEVICE);

  /*
   * Test routines.
   * Uncomment the one you need to exec.
   */

  /* Test sensor samples acquisition */
  Loop_Test_Sample_Aquisition();

  /* Test Pedometer */
  // Loop_Test_Pedometer();
  
  /* Test Wakup */
  // Loop_Test_Wakeup();
#endif
  return 0;
}

void sensor_test(uint8_t* data)
{
    response =  LSM6DS3_ACC_GYRO_R_XLDA(&value_XL);
    if(response==MEMS_ERROR) while(1); //manage here comunication error  
    
    if (LSM6DS3_ACC_GYRO_XLDA_DATA_AVAIL==value_XL)
    {
      LSM6DS3_ACC_GYRO_Get_GetAccData(data);
    }
}

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
