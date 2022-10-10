#include "iap.h"

/*********************************************************************
 * @fn      SetSysClock
 *
 * @brief   ����ϵͳ����ʱ��60Mhz, 0x48
 *
 * @param   sc      - ϵͳʱ��Դѡ�� refer to SYS_CLKTypeDef
 *
 * @return  none
 */
__HIGH_CODE void mySetSysClock()
{
    sys_safe_access_enable();
    R8_PLL_CONFIG &= ~(1 << 5); //
    sys_safe_access_disable();
    // PLL div
    if (!(R8_HFCK_PWR_CTRL & RB_CLK_PLL_PON)) {
        sys_safe_access_enable();
        R8_HFCK_PWR_CTRL |= RB_CLK_PLL_PON; // PLL power on
        for (uint32_t i = 0; i < 2000; i++) {
            __nop();
            __nop();
        }
    }
    sys_safe_access_enable();
    R16_CLK_SYS_CFG = (1 << 6) | (CLK_SOURCE_PLL_60MHz & 0x1f);
    __nop();
    __nop();
    __nop();
    __nop();
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_FLASH_CFG = 0X52;
    sys_safe_access_disable();
    //����FLASH clk����������
    sys_safe_access_enable();
    R8_PLL_CONFIG |= 1 << 7;
    sys_safe_access_disable();
}

/*********************************************************************
 * @fn      Main_Circulation
 *
 * @brief   IAP��ѭ��,�����ram�����У������ٶ�.
 *
 * @param   None.
 *
 * @return  None.
 */
__HIGH_CODE void Main_Circulation()
{
    uint32_t j = 0;

    while (1) {
        j++;
        if (j > 5) //100us����һ������
        {
            j = 0;
            USB_DevTransProcess(); //���ò�ѯ��ʽ����usb��������ʹ���жϡ�
        }
        DelayUs(20);
        g_tcnt++;
        if (g_tcnt > 3000000) {
            //1����û�в���������app
            jumpAppPre;
            jumpApp();
        }
    }
}

/*********************************************************************
 * @fn      main
 *
 * @brief   ������
 *
 * @return  none
 */
int main()
{
#if (defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    uint16_t adj = R16_AUX_POWER_ADJ;
    uint16_t plan = R16_POWER_PLAN;

    adj |= RB_DCDC_CHARGE;
    plan |= RB_PWR_DCDC_PRE;
    sys_safe_access_enable();
    R16_AUX_POWER_ADJ = adj;
    R16_POWER_PLAN = plan;
    DelayUs(10);
    sys_safe_access_enable();
    R16_POWER_PLAN |= RB_PWR_DCDC_EN;
    sys_safe_access_disable();
#endif
    mySetSysClock(); //Ϊ�˾������������ú�������ͨ��ĳ�ʼ���������޸ģ�ֻ���Խ�ʱ������Ϊ60M
    DelayMs(5);
#ifdef PLF_DEBUG
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    UART1_BaudRateCfg(460800);
#endif

    PRINT("Chip start, %s\n", VER_LIB);
    PRINT("Reason of last reset:  ");
    switch (R8_RESET_STATUS & RB_RESET_FLAG) {
        case 0b000:
            PRINT("Software\n");
            break;
        case 0b001:
            PRINT("Power on\n");
            break;
        case 0b010:
            PRINT("Watchdog timeout\n");
            break;
        case 0b011:
            PRINT("Manual\n");
            break;
        case 0b101:
            PRINT("Wake from shutdown\n");
            break;
        default:
            PRINT("Wake from sleep\n");
            break;
    }

#ifdef BOOTMAGIC_ENABLE
    PRINT("Bootmagic!\n");

#if !defined ESB_ENABLE || ESB_ENABLE != 2
    bool jump_app = false;
    pin_t rows[] = MATRIX_ROW_PINS;
    pin_t cols[] = MATRIX_COL_PINS;

#if DIODE_DIRECTION == COL2ROW
    pin_t input_pin = cols[BOOTMAGIC_LITE_COLUMN];
    pin_t output_pin = rows[BOOTMAGIC_LITE_ROW];
#else
    pin_t input_pin = rows[BOOTMAGIC_LITE_ROW];
    pin_t output_pin = cols[BOOTMAGIC_LITE_COLUMN];
#endif
    setPinInputHigh(input_pin);
    writePinLow(output_pin);
    setPinOutput(output_pin);
    do {
        if (readPin(input_pin)) {
            jump_app = true;
            break;
        }
        DelayMs(DEBOUNCE * 3);
        if (readPin(input_pin)) {
            jump_app = true;
            break;
        }
    } while (0);
    if (jump_app) {
        PRINT("Entering APP...\n");
        jumpApp();
    } else {
        PRINT("Entering DFU...\n");
        eeprom_driver_erase();
    }
#else
    // TODO: implement judging condition for 2.4g dongle
#endif
#else
    PRINT("Entering APP...\n");
    jumpApp();
#endif

    /* USB��ʼ�� */
    R8_USB_CTRL = 0x00; // ���趨ģʽ,ȡ�� RB_UC_CLR_ALL

    R8_UEP4_1_MOD = RB_UEP4_RX_EN | RB_UEP4_TX_EN | RB_UEP1_RX_EN | RB_UEP1_TX_EN; // �˵�4 OUT+IN,�˵�1 OUT+IN
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN | RB_UEP3_RX_EN | RB_UEP3_TX_EN; // �˵�2 OUT+IN,�˵�3 OUT+IN

    R16_UEP0_DMA = (uint16_t)(uint32_t)EP0_Databuf;
    R16_UEP1_DMA = (uint16_t)(uint32_t)EP1_Databuf;
    R16_UEP2_DMA = (uint16_t)(uint32_t)EP2_Databuf;
    R16_UEP3_DMA = (uint16_t)(uint32_t)EP3_Databuf;

    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP4_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;

    R8_USB_DEV_AD = 0x00;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN; // ����USB�豸��DMA�����ж��ڼ��жϱ�־δ���ǰ�Զ�����NAK
    R16_PIN_ANALOG_IE |= RB_PIN_USB_IE | RB_PIN_USB_DP_PU;         // ��ֹUSB�˿ڸ��ռ���������
    R8_USB_INT_FG = 0xFF;                                          // ���жϱ�־
    R8_UDEV_CTRL = RB_UD_PD_DIS | RB_UD_PORT_EN;                   // ����USB�˿�
    R8_USB_INT_EN = 0;                                             //��ֹusb�жϣ����ò�ѯ��ʽ

    /* ����highcode��ѭ�� */
    Main_Circulation();
}
