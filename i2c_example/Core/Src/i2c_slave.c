/*
 * i2c_slave.c
 *
 *  Created on: Mar 3, 2024
 *      Author: dw
 */
#include "main.h"
#include "i2c_slave.h"

static uint32_t register_read (void);
static void register_update (void);

enum { 	I2C_IDLE,
		I2C_RX_REGISTER_ADDRESS,
		I2C_RX_DATA
} i2c_state = I2C_IDLE;

volatile 	uint32_t	registers[MAX_REGS] = { 0 };
			uint8_t		registerAddress;
			uint8_t		registerByteCount;
			uint32_t	registerValue;
			uint32_t	registerReadOnlyFlag = 0;
			callback    registerCallbacks[MAX_REGS] = {0};


/*
 * Marks a register read-only and sets up the value for it
 */
void I2C_Setup_Readonly_Register (int reg, uint32_t value)
{
	if (reg < MAX_REGS) {
		registers[reg] = value;
		registerReadOnlyFlag |= (1 << reg);
	}
}


/*
 * Sets up a callback function for a register so that it
 * can process the data change.
 */
void I2C_Setup_Callback_Register (int reg, callback cb)
{
	if (reg < MAX_REGS) {
		registerCallbacks[reg] = cb;
	}
}


/**
  * @brief  Listen Complete callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval None
  */
void HAL_I2C_ListenCpltCallback (I2C_HandleTypeDef *hi2c)
{
	HAL_I2C_EnableListen_IT (hi2c);
}


/**
  * @brief  Slave Address Match callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @param  TransferDirection Master request Transfer Direction (Write/Read), value of @ref I2C_XFERDIRECTION
		#define I2C_DIRECTION_TRANSMIT          (0x00000000U)
		#define I2C_DIRECTION_RECEIVE           (0x00000001U)

  * @param  AddrMatchCode Address Match Code
  * @retval None
  */
void HAL_I2C_AddrCallback (I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
	/* Prevent unused argument(s) compilation warning */
	UNUSED(AddrMatchCode);

	if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
		i2c_state = I2C_RX_REGISTER_ADDRESS;
		HAL_I2C_Slave_Sequential_Receive_IT (hi2c, &registerAddress, 1, I2C_FIRST_FRAME);
	} else {
		registerValue = register_read();
		HAL_I2C_Slave_Sequential_Transmit_IT (hi2c, (uint8_t*) &registerValue, REG_SIZE, I2C_FIRST_AND_LAST_FRAME);
	}
}


/**
  * @brief  Slave Rx Transfer completed callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval None
  */
void HAL_I2C_SlaveRxCpltCallback (I2C_HandleTypeDef *hi2c)
{
	if (i2c_state == I2C_RX_REGISTER_ADDRESS) {
		i2c_state = I2C_RX_DATA;
		registerByteCount = 0;
		registerValue = 0;
		HAL_I2C_Slave_Sequential_Receive_IT (hi2c, (uint8_t*) &registerValue, 1, I2C_NEXT_FRAME);
	} else {
		registerByteCount++;
		if (registerByteCount < REG_SIZE)
		{
			int option = (registerByteCount == REG_SIZE-1) ? I2C_LAST_FRAME : I2C_NEXT_FRAME;
			HAL_I2C_Slave_Sequential_Receive_IT(hi2c, ((uint8_t*) registerValue) + registerByteCount, 1, option);
		} else {
			i2c_state = I2C_IDLE;
			register_update ();
		}
	}
}


/**
  * @brief  I2C error callback.
  * @param  hi2c Pointer to a I2C_HandleTypeDef structure that contains
  *                the configuration information for the specified I2C.
  * @retval None
  */
void HAL_I2C_ErrorCallback (I2C_HandleTypeDef *hi2c)
{
	uint32_t errorcode = HAL_I2C_GetError (hi2c);
	if (errorcode == HAL_I2C_ERROR_AF) { // AckFlag error
		if (i2c_state == I2C_RX_DATA && registerByteCount > 0) {
			register_update();
		}
	}
	i2c_state = I2C_IDLE;
	HAL_I2C_EnableListen_IT (hi2c);
}


/*
 * Called before sending data back to the Pi in case we need to update
 * the data before sending.  We could use a "read callback" in the
 * general case - here we assume the data is current in the volatile register.
 */
static uint32_t register_read (void)
{
	if (registerAddress < MAX_REGS)
		return registers[registerAddress];
	return 0xFFFFFFFF;
}


/*
 * Called after receiving a new data values. Checks the address, stores the
 * data and calls the callback if there is one.
 */
static void register_update (void)
{
	if (registerAddress < MAX_REGS) {
		if ((registerReadOnlyFlag & (1 << registerAddress)) == 0) {
			registers[registerAddress] = registerValue;
		}
		if (registerCallbacks[registerAddress] != NULL) {
			(registerCallbacks[registerAddress])();
		}
	}
}

