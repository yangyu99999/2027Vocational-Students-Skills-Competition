#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "MatrixKey.h"
#include "Key.h"

uint8_t KeyNum;

int main(void)
{
	OLED_Init();
	MatrixKey_Init();
	OLED_ShowString(1, 1, "KeyNum:");
		  
	while (1)
	{
		KeyNum = MatrixKey_GetValue();
		OLED_ShowNum(1, 9, KeyNum, 2);
		
	}
}
