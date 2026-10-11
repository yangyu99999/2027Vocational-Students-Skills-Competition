#include "FLASH.h"

/**
  * @brief  读取指定地址的字 (32-bit)
  */
uint32_t Flash_ReadWord(uint32_t Address)
{
    if (Address < FLASH_START_ADDR || Address > FLASH_END_ADDR-4U || (Address & 3U)) return 0xFFFFFFFFU;
    return *(__IO uint32_t *)Address;
}

/**
  * @brief  读取指定地址的半字 (16-bit)
  */
uint16_t Flash_ReadHalfWord(uint32_t Address)
{
    if (Address < FLASH_START_ADDR || Address > FLASH_END_ADDR-2U || (Address & 1U)) return 0xFFFFU;
    return *(__IO uint16_t *)Address;
}

/**
  * @brief  获取地址所在的页码
  */
uint32_t Flash_GetPage(uint32_t Address)
{
    if (Address < FLASH_START_ADDR || Address >= FLASH_END_ADDR) return 0xFFFFFFFFU;
    return (Address - FLASH_START_ADDR) / FLASH_PAGE_SIZE;
}

/**
  * @brief  擦除指定的 Flash 页
  */
uint8_t Flash_ErasePage(uint32_t PageAddress)
{
    // 检查地址有效性
    if (PageAddress < FLASH_START_ADDR || PageAddress >= FLASH_END_ADDR || ((PageAddress-FLASH_START_ADDR) % FLASH_PAGE_SIZE)!=0U) {
        return FLASH_ERR_ADDR_OUT;
    }

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;
    HAL_StatusTypeDef status;

    // 解锁 Flash
    if (HAL_FLASH_Unlock() != HAL_OK) return FLASH_ERR_WRITE;

    // 填充擦除结构体
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = PageAddress;
    EraseInitStruct.NbPages     = 1; // 每次擦除1页

    // 调用 HAL 库擦除函数
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    // 锁定 Flash
    if (HAL_FLASH_Lock() != HAL_OK) return FLASH_ERR_WRITE;

    if (status != HAL_OK) {
        return FLASH_ERR_ERASE;
    }

    return FLASH_OK;
}

/**
  * @brief  在指定地址写入一个字 (32-bit)
  */
uint8_t Flash_WriteWord(uint32_t Address, uint32_t Data)
{
    HAL_StatusTypeDef status;

    // 检查地址有效性
    if (Address < FLASH_START_ADDR || Address > FLASH_END_ADDR-4U || (Address & 3U)!=0U) {
        return FLASH_ERR_ADDR_OUT;
    }

    // 解锁 Flash
    if (HAL_FLASH_Unlock() != HAL_OK) return FLASH_ERR_WRITE;

    /* STM32F1 系列实际上是按半字(16-bit)编程的，
       但 HAL_FLASH_Program 使用 FLASH_TYPEPROGRAM_WORD 会自动处理两次16位写入 */
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, (uint64_t)Data);

    // 锁定 Flash
    if (HAL_FLASH_Lock() != HAL_OK) return FLASH_ERR_WRITE;

    if (status != HAL_OK) {
        return FLASH_ERR_WRITE;
    }

    return FLASH_OK;
}

/**
  * @brief  在指定地址写入一组字
  */
uint8_t Flash_WriteArray(uint32_t StartAddress, uint32_t *pData, uint16_t Length)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t currentAddr = StartAddress;
    uint16_t i;

    if (pData == NULL || Length == 0U) return FLASH_ERR_ARGUMENT;
    // 检查地址范围
    if (StartAddress < FLASH_START_ADDR || StartAddress >= FLASH_END_ADDR || (StartAddress & 3U)!=0U || Length > (FLASH_END_ADDR-StartAddress)/4U) {
        return FLASH_ERR_ADDR_OUT;
    }

    // 解锁 Flash (在循环外解锁以提高效率)
    if (HAL_FLASH_Unlock() != HAL_OK) return FLASH_ERR_WRITE;

    for (i = 0; i < Length; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, currentAddr, (uint64_t)pData[i]);
        
        if (status != HAL_OK) {
            break; // 发生错误，退出循环
        }
        
        currentAddr += 4; // 地址偏移 4 字节
    }

    // 锁定 Flash
    if (HAL_FLASH_Lock() != HAL_OK) return FLASH_ERR_WRITE;

    if (status != HAL_OK) {
        return FLASH_ERR_WRITE;
    }

    return FLASH_OK;
}
