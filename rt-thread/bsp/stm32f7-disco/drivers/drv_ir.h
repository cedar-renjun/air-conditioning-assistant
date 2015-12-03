#ifndef _DRV_IR_H_
#define _DRV_IR_H_

#ifndef bool
#define bool int
#endif  

#ifndef true
#define true 1
#endif  
#ifndef false
#define false 0
#endif  


typedef enum{IR_IDL = 0, IR_SND, IR_REC}irWorkMode_t;

int irmpHwInit(void);
int irmpInit(void);

#endif // _DRV_IR_H_
