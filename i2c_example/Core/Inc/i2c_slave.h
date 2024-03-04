/*
 * i2c_slave.h
 *
 *  Created on: Mar 3, 2024
 *      Author: dw
 */

#ifndef INC_I2C_SLAVE_H_
#define INC_I2C_SLAVE_H_


#define MAX_REGS		(32)
#define REG_SIZE		(4)

extern volatile uint32_t registers[];

typedef void (*callback) (void);

#define REGISTER(n)			registers[n]

void I2C_Setup_Readonly_Register (int reg, uint32_t value);
void I2C_Setup_Callback_Register (int reg, callback cb);

#endif /* INC_I2C_SLAVE_H_ */
