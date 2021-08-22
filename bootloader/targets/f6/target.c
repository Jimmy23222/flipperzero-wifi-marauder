#include <target.h>
#include <stm32wbxx.h>
#include <stm32wbxx_ll_system.h>
#include <stm32wbxx_ll_bus.h>
#include <stm32wbxx_ll_utils.h>
#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_rtc.h>
#include <stm32wbxx_ll_pwr.h>
#include <stm32wbxx_ll_gpio.h>
#include <stm32wbxx_hal_flash.h>

#include <lib/toolbox/version.h>
#include <furi-hal.h>
#include <u8g2.h>

const uint8_t I_Warning_30x23_0[] = {
    0x00, 0xC0, 0x00, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00, 0xF0, 0x03, 0x00, 0x00, 0xF0, 0x03, 0x00,
    0x00, 0xF8, 0x07, 0x00, 0x00, 0x3C, 0x0F, 0x00, 0x00, 0x3C, 0x0F, 0x00, 0x00, 0x3E, 0x1F, 0x00,
    0x00, 0x3F, 0x3F, 0x00, 0x00, 0x3F, 0x3F, 0x00, 0x80, 0x3F, 0x7F, 0x00, 0xC0, 0x3F, 0xFF, 0x00,
    0xC0, 0x3F, 0xFF, 0x00, 0xE0, 0x3F, 0xFF, 0x01, 0xF0, 0x3F, 0xFF, 0x03, 0xF0, 0x3F, 0xFF, 0x03,
    0xF8, 0x3F, 0xFF, 0x07, 0xFC, 0xFF, 0xFF, 0x0F, 0xFC, 0xFF, 0xFF, 0x0F, 0xFE, 0x3F, 0xFF, 0x1F,
    0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0xFF, 0xFF, 0x3F, 0xFE, 0xFF, 0xFF, 0x1F,
};

// Boot request enum
#define BOOT_REQUEST_TAINTED 0x00000000
#define BOOT_REQUEST_CLEAN 0xDADEDADE
#define BOOT_REQUEST_DFU 0xDF00B000
// Boot to DFU pin
#define BOOT_DFU_PORT GPIOB
#define BOOT_DFU_PIN LL_GPIO_PIN_11
// USB pins
#define BOOT_USB_PORT GPIOA
#define BOOT_USB_DM_PIN LL_GPIO_PIN_11
#define BOOT_USB_DP_PIN LL_GPIO_PIN_12
#define BOOT_USB_PIN (BOOT_USB_DM_PIN | BOOT_USB_DP_PIN)

#define RTC_CLOCK_IS_READY() (LL_RCC_LSE_IsReady() && LL_RCC_LSI1_IsReady())

uint8_t u8g2_gpio_and_delay_stm32(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);
uint8_t u8x8_hw_spi_stm32(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr);

void target_led_control(char* c) {
    furi_hal_light_set(LightRed, 0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue, 0x00);
    do {
        if(*c == 'R') {
            furi_hal_light_set(LightRed, 0xFF);
        } else if(*c == 'G') {
            furi_hal_light_set(LightGreen, 0xFF);
        } else if(*c == 'B') {
            furi_hal_light_set(LightBlue, 0xFF);
        } else if(*c == '.') {
            LL_mDelay(125);
            furi_hal_light_set(LightRed, 0x00);
            furi_hal_light_set(LightGreen, 0x00);
            furi_hal_light_set(LightBlue, 0x00);
            LL_mDelay(125);
        } else if(*c == '-') {
            LL_mDelay(250);
            furi_hal_light_set(LightRed, 0x00);
            furi_hal_light_set(LightGreen, 0x00);
            furi_hal_light_set(LightBlue, 0x00);
            LL_mDelay(250);
        } else if(*c == '|') {
            furi_hal_light_set(LightRed, 0x00);
            furi_hal_light_set(LightGreen, 0x00);
            furi_hal_light_set(LightBlue, 0x00);
        }
        c++;
    } while(*c != 0);
}

void target_clock_init() {
    LL_Init1msTick(4000000);
    LL_SetSystemCoreClock(4000000);

    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOD);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOE);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOH);

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2);
}

void target_gpio_init() {
    // USB D+
    LL_GPIO_SetPinMode(BOOT_USB_PORT, BOOT_USB_DP_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinSpeed(BOOT_USB_PORT, BOOT_USB_DP_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinOutputType(BOOT_USB_PORT, BOOT_USB_DP_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
    // USB D-
    LL_GPIO_SetPinMode(BOOT_USB_PORT, BOOT_USB_DM_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinSpeed(BOOT_USB_PORT, BOOT_USB_DM_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinOutputType(BOOT_USB_PORT, BOOT_USB_DM_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
    // Button: back
    LL_GPIO_SetPinMode(BOOT_DFU_PORT, BOOT_DFU_PIN, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(BOOT_DFU_PORT, BOOT_DFU_PIN, LL_GPIO_PULL_UP);
}

void target_rtc_init() {
    // LSE and RTC
    LL_PWR_EnableBkUpAccess();
    if(!RTC_CLOCK_IS_READY()) {
        // Start LSI1 needed for CSS
        LL_RCC_LSI1_Enable();
        // Try to start LSE normal way
        LL_RCC_LSE_SetDriveCapability(LL_RCC_LSEDRIVE_HIGH);
        LL_RCC_LSE_Enable();
        uint32_t c = 0;
        while(!RTC_CLOCK_IS_READY() && c < 200) {
            LL_mDelay(10);
            c++;
        }
        // Plan B: reset backup domain
        if(!RTC_CLOCK_IS_READY()) {
            target_led_control("-R.R.R.");
            LL_RCC_ForceBackupDomainReset();
            LL_RCC_ReleaseBackupDomainReset();
            NVIC_SystemReset();
        }
        // Set RTC domain clock to LSE
        LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);
        // Enable LSE CSS
        LL_RCC_LSE_EnableCSS();
    }
    // Enable clocking
    LL_RCC_EnableRTC();
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTCAPB);
}

void target_version_save(void) {
    LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR1, (uint32_t)version_get());
}

void target_usb_wire_reset() {
    LL_GPIO_ResetOutputPin(BOOT_USB_PORT, BOOT_USB_PIN);
}

void target_display_init() {
    // Prepare gpio
    hal_gpio_init_simple(&gpio_display_rst, GpioModeOutputPushPull);
    hal_gpio_init_simple(&gpio_display_di, GpioModeOutputPushPull);
    // Initialize
    u8g2_t fb;
    u8g2_Setup_st7565_erc12864_alt_f(&fb, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32);
    u8g2_InitDisplay(&fb);
    u8g2_SetContrast(&fb, 36);
    // Create payload
    u8g2_ClearBuffer(&fb);
    u8g2_SetDrawColor(&fb, 0x01);
    u8g2_SetFont(&fb, u8g2_font_helvB08_tf);
    u8g2_DrawStr(&fb, 2, 8, "Recovery & Update Mode");
    u8g2_DrawXBM(&fb, 49, 14, 30, 23, I_Warning_30x23_0);
    u8g2_DrawStr(&fb, 2, 50, "DFU Bootloader activated");
    u8g2_DrawStr(&fb, 6, 62, "www.flipp.dev/recovery");
    // Send buffer
    u8g2_SetPowerSave(&fb, 0);
    u8g2_SendBuffer(&fb);
}

void target_init() {
    target_clock_init();
    target_gpio_init();
    furi_hal_init();
    target_led_control("RGB");
    target_rtc_init();
    target_version_save();
    target_usb_wire_reset();

    // Errata 2.2.9, Flash OPTVERR flag is always set after system reset
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
}

int target_is_dfu_requested() {
    if(LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR0) == BOOT_REQUEST_TAINTED) {
        // Default system state is tainted
        // We must ensure that MCU is cleanly booted
        LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR0, BOOT_REQUEST_CLEAN);
        NVIC_SystemReset();
    } else if(LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR0) == BOOT_REQUEST_DFU) {
        return 1;
    }
    LL_mDelay(100);
    if(!LL_GPIO_IsInputPinSet(BOOT_DFU_PORT, BOOT_DFU_PIN)) {
        return 1;
    }

    return 0;
}

void target_switch(void* offset) {
    asm volatile("ldr    r3, [%0]    \n"
                 "msr    msp, r3     \n"
                 "ldr    r3, [%1]    \n"
                 "mov    pc, r3      \n"
                 :
                 : "r"(offset), "r"(offset + 0x4)
                 : "r3");
}

void target_switch2dfu() {
    target_led_control("B");
    furi_hal_light_set(LightBacklight, 0xFF);
    target_display_init();
    // Mark system as tainted, it will be soon
    LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR0, BOOT_REQUEST_TAINTED);
    // Remap memory to system bootloader
    LL_SYSCFG_SetRemapMemory(LL_SYSCFG_REMAP_SYSTEMFLASH);
    // Jump
    target_switch(0x0);
}

void target_switch2os() {
    target_led_control("G");
    SCB->VTOR = OS_OFFSET;
    target_switch((void*)(BOOT_ADDRESS + OS_OFFSET));
}
