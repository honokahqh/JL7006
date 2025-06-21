#ifndef __POWER_PORT_H__
#define __POWER_PORT_H__

#define     _PORT(p)            JL_PORT##p
#define     _PORT_IN(p,b)       P##p##b##_IN
#define     _PORT_OUT(p,b)      JL_OMAP->P##p##b##_OUT

#define     SPI_PORT(p)         _PORT(p)
#define     SPI0_FUNC_OUT(p,b)  _PORT_OUT(p,b)
#define     SPI0_FUNC_IN(p,b)   _PORT_IN(p,b)

// | func\port |  A   |  B   |
// |-----------|------|------|
// | CS        | PD3  | PD5  |
// | CLK       | PD0  | PD0  |
// | DO(D0)    | PD1  | PD1  |
// | DI(D1)    | PD2  | PD6  |
// | WP(D2)    | PC6  | PC6  |
// | HOLD(D3)  | PA7  | PA7  |

////////////////////////////////////////////////////////////////////////////////
#define     PORT_SPI0_PWRA      D
#define     SPI0_PWRA           4

#define     PORT_SPI0_CSA       D
#define     SPI0_CSA            3

#define     PORT_SPI0_CLKA      D
#define     SPI0_CLKA           0

#define     PORT_SPI0_DOA       D
#define     SPI0_DOA            1

#define     PORT_SPI0_DIA       D
#define     SPI0_DIA            2

#define     PORT_SPI0_D2A       C
#define     SPI0_D2A            6

#define     PORT_SPI0_D3A       A
#define     SPI0_D3A            7

////////////////////////////////////////////////////////////////////////////////
#define     PORT_SPI0_PWRB      D
#define     SPI0_PWRB           4

#define     PORT_SPI0_CSB       D
#define     SPI0_CSB            5

#define     PORT_SPI0_CLKB      D
#define     SPI0_CLKB           0

#define     PORT_SPI0_DOB       D
#define     SPI0_DOB            1

#define     PORT_SPI0_DIB       D
#define     SPI0_DIB            6

#define     PORT_SPI0_D2B       C
#define     SPI0_D2B            6

#define     PORT_SPI0_D3B       A
#define     SPI0_D3B            7

#define USB_CON0        JL_USB->CON0
#define USB_IO_CON0     JL_USB_IO->CON0

#define USB_DP_OUT(x)   USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(0)) | ((x & 0x01) << 0))
#define USB_DM_OUT(x)   USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(1)) | ((x & 0x01) << 1))
#define USB_DP_DIR(x)   USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(2)) | ((x & 0x01) << 2))
#define USB_DM_DIR(x)   USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(3)) | ((x & 0x01) << 3))
#define USB_DP_PU(x)    USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(4)) | ((x & 0x01) << 4))
#define USB_DM_PU(x)    USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(5)) | ((x & 0x01) << 5))
#define USB_DP_PD(x)    USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(6)) | ((x & 0x01) << 6))
#define USB_DM_PD(x)    USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(7)) | ((x & 0x01) << 7))
#define USB_DP_DIE(x)   USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(8)) | ((x & 0x01) << 8))
#define USB_DM_DIE(x)   USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(9)) | ((x & 0x01) << 9))
#define USB_DP_DIEH(x)  USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(10)) | ((x & 0x01) << 10))
#define USB_DM_DIEH(x)  USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(11)) | ((x & 0x01) << 11))
#define USB_IO_MODE(x)  USB_IO_CON0 = ((USB_IO_CON0 & ~BIT(14)) | ((x & 0x01) << 14))

void port_init(void);

u8 get_sfc_bit_mode();

#endif
