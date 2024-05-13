/***************************************************************************/ /**
 * @file gspi_example.c
 * @brief GSPI examples functions
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include "cmsis_os2.h"
#include "sl_si91x_gspi.h"
#include "sl_si91x_gspi_common_config.h"
#include "gspi_example.h"
#include "rsi_debug.h"
#include <rsi_common_apis.h>

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define GSPI_BUFFER_SIZE             20480      // Size of buffer
#define GSPI_INTF_PLL_CLK            180000000 // Intf pll clock frequency
#define GSPI_INTF_PLL_REF_CLK        40000000  // Intf pll reference clock frequency
#define GSPI_SOC_PLL_CLK             20000000  // Soc pll clock frequency
#define GSPI_SOC_PLL_REF_CLK         40000000  // Soc pll reference clock frequency
#define GSPI_INTF_PLL_500_CTRL_VALUE 0xD900    // Intf pll control value
#define GSPI_SOC_PLL_MM_COUNT_LIMIT  0xA4      // Soc pll count limit
#define GSPI_DVISION_FACTOR          0         // Division factor
#define GSPI_SWAP_READ_DATA          1         // true to enable and false to disable swap read
#define GSPI_SWAP_WRITE_DATA         0         // true to enable and false to disable swap write
#define GSPI_BITRATE                 40000000  // Bitrate for setting the clock division factor
#define GSPI_BIT_WIDTH               8         // Default Bit width
#define GSPI_MAX_BIT_WIDTH           16        // Maximum Bit width

/*******************************************************************************
 *************************** LOCAL VARIABLES   *******************************
 ******************************************************************************/
uint16_t gspi_data_in[GSPI_BUFFER_SIZE];
uint16_t gspi_data_out[GSPI_BUFFER_SIZE];
static uint16_t gspi_division_factor       = 1;
static sl_gspi_handle_t gspi_driver_handle = NULL;

uint8_t gspi_init = 0;
extern osSemaphoreId_t gspi_thread_sem;

//Enum for different transmission scenarios
typedef enum {
  SL_GSPI_TRANSFER_DATA,
  SL_GSPI_RECEIVE_DATA,
  SL_GSPI_SEND_DATA,
  SL_GSPI_TRANSMISSION_COMPLETED,
} gspi_mode_enum_t;
static gspi_mode_enum_t current_mode = SL_GSPI_TRANSFER_DATA;

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
static sl_status_t init_clock_configuration_structure(sl_gspi_clock_config_t *clock_config);
static void compare_loop_back_data(void);
static void callback_event(uint32_t event);
static boolean_t transfer_complete  = false;
static boolean_t begin_transmission = true;

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

void gspi_master_task(void *argument)
{
  UNUSED_PARAMETER(argument);
  while(1){
    if(gspi_init)
    {
      //LOG_PRINT("current SSI state: %d\r\n", ssi_master_current_mode);
      gspi_example_process_action();
    }
    else
    {
      LOG_PRINT("GSPI not be initialed, Wait for SEM release\r\n");
      osSemaphoreAcquire(gspi_thread_sem, osWaitForever); // wait ssi init done
    }
  }
}
/*******************************************************************************
 * GSPI example initialization function
 ******************************************************************************/
void gspi_example_init(void)
{
  sl_status_t status;
  sl_gspi_clock_config_t clock_config;
  sl_gspi_version_t version;
  sl_gspi_status_t gspi_status;
  sl_gspi_control_config_t config;
  config.bit_width         = GSPI_BIT_WIDTH;
  config.bitrate           = GSPI_BITRATE;
  config.clock_mode        = SL_GSPI_MODE_0;
  config.slave_select_mode = SL_GSPI_MASTER_HW_OUTPUT;
  config.swap_read         = GSPI_SWAP_READ_DATA;
  config.swap_write        = GSPI_SWAP_WRITE_DATA;

  // Filling the data out array with integer values
  for (uint16_t i = 0; i < GSPI_BUFFER_SIZE; i++) {
    gspi_data_out[i] = i;
  }
  do {
    // Version information of GSPI driver
    version = sl_si91x_gspi_get_version();
    LOG_PRINT("GSPI version is fetched successfully \n");
    LOG_PRINT("API version is %d.%d.%d\n", version.release, version.major, version.minor);
#if 0// USER_TEST_GSPI && !(USER_TEST_SSI)
    // Filling up the structure with the default clock parameters
    status = init_clock_configuration_structure(&clock_config);
    if (status != SL_STATUS_OK) {
      LOG_PRINT("init_clock_configuration_structure: Error Code : %lu \n", status);
      break;
    }
    // Configuration of clock with the default clock parameters
    status = sl_si91x_gspi_configure_clock(&clock_config);
    if (status != SL_STATUS_OK) {
      LOG_PRINT("sl_si91x_gspi_clock_configuration: Error Code : %lu \n", status);
      break;
    }
    LOG_PRINT("Clock configuration is successful \n");
#endif
    // Pass the address of void pointer, it will be updated with the address
    // of GSPI instance which can be used in other APIs.
    status = sl_si91x_gspi_init(SL_GSPI_MASTER, &gspi_driver_handle);
    if (status != SL_STATUS_OK) {
      LOG_PRINT("sl_si91x_gspi_init: Error Code : %lu \n", status);
      break;
    }
    LOG_PRINT("GSPI initialization is successful \n");
    // Fetching the status of GSPI i.e., busy, data lost and mode fault
    gspi_status = sl_si91x_gspi_get_status(gspi_driver_handle);
    LOG_PRINT("GSPI status is fetched successfully \n");
    LOG_PRINT("Busy: %d\n", gspi_status.busy);
    LOG_PRINT("Data_Lost: %d\n", gspi_status.data_lost);
    LOG_PRINT("Mode_Fault: %d\n", gspi_status.mode_fault);
    //Configuration of all other parameters that are required by GSPI
    // gspi_configuration structure is from sl_si91x_gspi_init.h file.
    // The user can modify this structure with the configuration of
    // his choice by filling this structure.
    status = sl_si91x_gspi_set_configuration(gspi_driver_handle, &config);
    if (status != SL_STATUS_OK) {
      LOG_PRINT("sl_si91x_gspi_control: Error Code : %lu \n", status);
      break;
    }
    LOG_PRINT("GSPI configuration is successful \n");
    // Register user callback function
    status = sl_si91x_gspi_register_event_callback(gspi_driver_handle, callback_event);
    if (status != SL_STATUS_OK) {
      LOG_PRINT("sl_si91x_gspi_register_event_callback: Error Code : %lu \n", status);
      break;
    }
    LOG_PRINT("GSPI user event callback registered successfully \n");
    // Fetching and printing the current clock division factor
    LOG_PRINT("Current Clock division factor is %lu \n", sl_si91x_gspi_get_clock_division_factor(gspi_driver_handle));
    // Fetching and printing the current frame length
    LOG_PRINT("Current Frame Length is %lu \n", sl_si91x_gspi_get_frame_length());
    if (sl_si91x_gspi_get_frame_length() > GSPI_BIT_WIDTH) {
      gspi_division_factor = sizeof(gspi_data_out[0]);
    }
    // As per the macros enabled in the header file, it will configure the current mode.


    current_mode = SL_GSPI_SEND_DATA;

    if (SL_USE_RECEIVE) {
      current_mode = SL_GSPI_RECEIVE_DATA;
      //break;
    }

    if (SL_USE_TRANSFER) {
      current_mode = SL_GSPI_TRANSFER_DATA;
      //break;
    }

    gspi_init = 1;
  } while (false);
}
/*******************************************************************************
 * Function will run continuously in while loop
 ******************************************************************************/
void gspi_example_process_action(void)
{
  sl_status_t status;
  static uint32_t rx_count = 0;
  static uint32_t tx_count = 0;
  // In this switch case, according to the macros enabled in header file, it starts to execute the APIs
  // Assuming all the macros are enabled, after transfer, receive will be executed and after receive
  // send will be executed.
  switch (current_mode) {
    case SL_GSPI_TRANSFER_DATA:
      if (begin_transmission == true) {
        // Validation for executing the API only once
        sl_si91x_gspi_set_slave_number(GSPI_SLAVE_0);
        while(1){
        status = sl_si91x_gspi_transfer_data(gspi_driver_handle,
                                             gspi_data_out,
                                             gspi_data_in,
                                             sizeof(gspi_data_out) / gspi_division_factor);
        }
        if (status != SL_STATUS_OK) {
          // If it fails to execute the API, it will not execute rest of the things
          LOG_PRINT("sl_si91x_gspi_transfer_data: Error Code : %lu \n", status);
          current_mode = SL_GSPI_TRANSMISSION_COMPLETED;
          break;
        }
        LOG_PRINT("GSPI transfer begin successfully \n");
        begin_transmission = false;
      }
      if (transfer_complete) {
        transfer_complete = false;
        LOG_PRINT("GSPI transfer completed successfully \n");
        // After comparing the loopback transfer, it compares the gspi_data_out and
        // gspi_data_in.
        compare_loop_back_data();
        if (SL_USE_RECEIVE) {
          // If receive macro is enabled, current mode is set to receive
          current_mode       = SL_GSPI_RECEIVE_DATA;
          begin_transmission = true;
          break;
        }
        if (SL_USE_SEND) {
          // If send macro is enabled, current mode is set to send
          current_mode       = SL_GSPI_SEND_DATA;
          begin_transmission = true;
          break;
        }
        // If above macros are not enabled, current mode is set to complete
        current_mode = SL_GSPI_TRANSMISSION_COMPLETED;
      }
      break;
    case SL_GSPI_RECEIVE_DATA:
      if (begin_transmission == true) {
        // Validation for executing the API only once
        sl_si91x_gspi_set_slave_number(GSPI_SLAVE_0);
        status = sl_si91x_gspi_receive_data(gspi_driver_handle, gspi_data_in, sizeof(gspi_data_in));
        if (status != SL_STATUS_OK) {
          // If it fails to execute the API, it will not execute rest of the things
          LOG_PRINT("sl_si91x_gspi_receive_data: Error Code : %lu \n", status);
          current_mode = SL_GSPI_TRANSMISSION_COMPLETED;
          break;
        }
        LOG_PRINT("GSPI receive begin successfully \n");
        begin_transmission = false;
        //Waiting till the receive is completed
      }
#if (SL_GSPI_DMA_CONFIG_ENABLE == ENABLE)
      if (transfer_complete) {
        // If DMA is enabled, it will wait untill transfer_complete flag is set.
        transfer_complete = false;
        if (SL_USE_SEND) {
          // If send macro is enabled, current mode is set to send
          current_mode       = SL_GSPI_SEND_DATA;
          begin_transmission = true;
          LOG_PRINT("GSPI receive completed \n");
          break;
        }
        LOG_PRINT("GSPI receive completed \n");
        // If send macro is not enabled, current mode is set to completed.
        current_mode = SL_GSPI_TRANSMISSION_COMPLETED;
      }
#else
      // If DMA is not enabled, it will wait till the rx_count is equal to number of
      // data to be received.
      if (rx_count < sizeof(gspi_data_in)) {
        rx_count = sl_si91x_gspi_get_rx_data_count(gspi_driver_handle);
        break;
      }
      if (SL_USE_SEND) {
        current_mode       = SL_GSPI_SEND_DATA;
        begin_transmission = true;
      }
#endif // SL_GSPI_DMA_CONFIG_ENABLE
      LOG_PRINT("GSPI receive completed \n");
      break;
    case SL_GSPI_SEND_DATA:
      if (begin_transmission) {
        // Validation for executing the API only once
        sl_si91x_gspi_set_slave_number(GSPI_SLAVE_0);
        status =
          sl_si91x_gspi_send_data(gspi_driver_handle, gspi_data_out, sizeof(gspi_data_out) / gspi_division_factor);
        if (status != SL_STATUS_OK) {
          // If it fails to execute the API, it will not execute rest of the things
          LOG_PRINT("sl_si91x_gspi_send_data: Error Code : %lu \n", status);
          current_mode = SL_GSPI_TRANSMISSION_COMPLETED;
          break;
        }
        LOG_PRINT("GSPI send begin successfully \n");
        begin_transmission = false;
      }
      //Waiting till the send is completed
#if (SL_GSPI_DMA_CONFIG_ENABLE == ENABLE)
      if (transfer_complete) {
        // If DMA is enabled, it will wait untill transfer_complete flag is set.
        transfer_complete = false;
        // At last current mode is set to completed.
        current_mode = SL_GSPI_TRANSMISSION_COMPLETED;
      }
#else
      // If DMA is not enabled, it will wait till the tx_count is equal to number of
      // data to be send.
      if (tx_count < sizeof(gspi_data_out)) {
        tx_count = sl_si91x_gspi_get_tx_data_count();
        break;
      }
      // At last current mode is set to completed.
      current_mode = SL_GSPI_TRANSMISSION_COMPLETED;
#endif // SL_GSPI_DMA_CONFIG_ENABLE
      LOG_PRINT("GSPI send completed \n");
      break;
    case SL_GSPI_TRANSMISSION_COMPLETED:
      break;
  }
  //LOG_PRINT("GSPI wait in the end of %s \r\n",(char*)__func__);
  //osSemaphoreAcquire(gspi_thread_sem, osWaitForever);
}

/*******************************************************************************
 * Function to fill the structure provided in argument by the default
 * clock paramteres
 * 
 * @param[in] clock_config pointer to the clock configuration structure
 * @return status (sl_status_t)
 *         SL_STATUS_NULL_POINTER (0x0022) - The parameter is null pointer.
 *         SL_STATUS_OK (0x0000) - Success
 ******************************************************************************/
static sl_status_t init_clock_configuration_structure(sl_gspi_clock_config_t *clock_config)
{
  sl_status_t status;
  if (clock_config == NULL) {
    status = SL_STATUS_NULL_POINTER;
  } else {
    clock_config->soc_pll_mm_count_value     = GSPI_SOC_PLL_MM_COUNT_LIMIT;
    clock_config->intf_pll_500_control_value = GSPI_INTF_PLL_500_CTRL_VALUE;
    clock_config->intf_pll_clock             = GSPI_INTF_PLL_CLK;
    clock_config->intf_pll_reference_clock   = GSPI_INTF_PLL_REF_CLK;
    clock_config->soc_pll_clock              = GSPI_SOC_PLL_CLK;
    clock_config->soc_pll_reference_clock    = GSPI_SOC_PLL_REF_CLK;
    clock_config->division_factor            = GSPI_DVISION_FACTOR;
    status                                   = SL_STATUS_OK;
  }
  return status;
}

/*******************************************************************************
 * Function to compare the loop back data, i.e., after transfer it will compare
 * the send and receive data
 * 
 * @param none
 * @return none
 ******************************************************************************/
static void compare_loop_back_data(void)
{
  // If the data width is not standard (8-bit) then the data should be masked.
  // The extra bits of the integer should be always zero.
  // For example, if bit width is 7, then from 8-15 all bits should be zero in a 16 bit integer.
  // So mask has value according to the data width and it is applied to the data.
  uint16_t data_index   = 0;
  uint32_t frame_length = 0;
  uint16_t mask         = (uint16_t)~0;
  frame_length          = sl_si91x_gspi_get_frame_length();
  mask                  = mask >> (GSPI_MAX_BIT_WIDTH - frame_length);
  for (data_index = 0; data_index < GSPI_BUFFER_SIZE; data_index++) {
    gspi_data_in[data_index] &= mask;
    gspi_data_out[data_index] &= mask;
    // If the data in and data out are same, it will be continued else, the for
    // loop will be break.
    if (gspi_data_in[data_index] != gspi_data_out[data_index]) {
      break;
    }
  }
  if (data_index == GSPI_BUFFER_SIZE) {
    LOG_PRINT("GSPI Data comparison successful, Loop Back Test Passed \n");
  } else {
    LOG_PRINT("GSPI Data comparison failed, Loop Back Test failed \n");
  }
}

/*******************************************************************************
 * Callback event function
 * It is responsible for the event which are triggered by GSPI interface
 * It updates the respective member of the structure as the event is triggered.
 ******************************************************************************/
static void callback_event(uint32_t event)
{
  switch (event) {
    case SL_GSPI_TRANSFER_COMPLETE:
      transfer_complete = true;
      LOG_PRINT("transfer_complete\r\n");
      break;
    case SL_GSPI_DATA_LOST:
      break;
    case SL_GSPI_MODE_FAULT:
      break;
  }
  LOG_PRINT("Release in %s\r\n", __func__);
  osSemaphoreRelease(gspi_thread_sem);
}
