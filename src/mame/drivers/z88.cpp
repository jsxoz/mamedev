// license:GPL-2.0+
// copyright-holders:Kevin Thacker,Sandro Ronco
/******************************************************************************

        z88.c

        z88 Notepad computer

        system driver

        TODO:
        - speaker controlled by txd
        - cartridges should be hot swappable
        - expansion interface
        - serial port

        Kevin Thacker [MESS driver]

*******************************************************************************/

#include "emu.h"
#include "includes/z88.h"

#include "softlist_dev.h"

/* Assumption:

all banks can access the same memory blocks in the same way.
bank 0 is special. If a bit is set in the com register,
the lower 8k is replaced with the rom. Bank 0 has been split
into 2 8k chunks, and all other banks into 16k chunks.
I wanted to handle all banks in the code below, and this
explains why the extra checks are done


    bank 0      0x0000-0x3FFF
    bank 1      0x4000-0x7FFF
    bank 2      0x8000-0xBFFF
    bank 3      0xC000-0xFFFF

    pages 0x00 - 0x1f   internal ROM
    pages 0x20 - 0x3f   512KB internal RAM
    pages 0x40 - 0x7f   Slot 1
    pages 0x80 - 0xbf   Slot 2
    pages 0xc0 - 0xff   Slot 3

*/

// cartridges read
uint8_t z88_state::bank0_cart_r(offs_t offset) { return m_carts[m_bank[0].slot]->read((m_bank[0].page<<14) + offset); }
uint8_t z88_state::bank1_cart_r(offs_t offset) { return m_carts[m_bank[1].slot]->read((m_bank[1].page<<14) + offset); }
uint8_t z88_state::bank2_cart_r(offs_t offset) { return m_carts[m_bank[2].slot]->read((m_bank[2].page<<14) + offset); }
uint8_t z88_state::bank3_cart_r(offs_t offset) { return m_carts[m_bank[3].slot]->read((m_bank[3].page<<14) + offset); }

// cartridges write
void z88_state::bank0_cart_w(offs_t offset, uint8_t data) { m_carts[m_bank[0].slot]->write((m_bank[0].page<<14) + offset, data); }
void z88_state::bank1_cart_w(offs_t offset, uint8_t data) { m_carts[m_bank[1].slot]->write((m_bank[1].page<<14) + offset, data); }
void z88_state::bank2_cart_w(offs_t offset, uint8_t data) { m_carts[m_bank[2].slot]->write((m_bank[2].page<<14) + offset, data); }
void z88_state::bank3_cart_w(offs_t offset, uint8_t data) { m_carts[m_bank[3].slot]->write((m_bank[3].page<<14) + offset, data); }


UPD65031_MEMORY_UPDATE(z88_state::bankswitch_update)
{
	// bank 0 is always even
	if (bank == 0)  page &= 0xfe;

	if (page < 0x20)    // internal ROM
	{
		// install read bank
		if (m_bank_type[bank] != Z88_BANK_ROM)
		{
			m_maincpu->space(AS_PROGRAM).install_read_bank(bank<<14, (bank<<14) + 0x3fff, m_banks[bank + 1]);
			m_maincpu->space(AS_PROGRAM).unmap_write(bank<<14, (bank<<14) + 0x3fff);
			m_bank_type[bank] = Z88_BANK_ROM;
		}

		m_banks[bank + 1]->set_entry(page);
	}
	else if (page < 0x40)   // internal RAM
	{
		if((page & 0x1f) < (m_ram->size()>>14))
		{
			// install readwrite bank
			if (m_bank_type[bank] != Z88_BANK_RAM)
			{
				m_maincpu->space(AS_PROGRAM).install_readwrite_bank(bank<<14, (bank<<14) + 0x3fff, m_banks[bank + 1]);
				m_bank_type[bank] = Z88_BANK_RAM;
			}

			// set the bank
			m_banks[bank + 1]->set_entry(page);
		}
		else
		{
			if (m_bank_type[bank] != Z88_BANK_UNMAP)
			{
				m_maincpu->space(AS_PROGRAM).unmap_readwrite(bank<<14, (bank<<14) + 0x3fff);
				m_bank_type[bank] = Z88_BANK_UNMAP;
			}
		}
	}
	else    // cartridges
	{
		m_bank[bank].slot = (page >> 6) & 3;
		m_bank[bank].page = page & 0x3f;

		if (m_bank_type[bank] != Z88_BANK_CART)
		{
			switch (bank)
			{
				case 0:
					m_maincpu->space(AS_PROGRAM).install_readwrite_handler(0x0000, 0x3fff, read8sm_delegate(*this, FUNC(z88_state::bank0_cart_r)), write8sm_delegate(*this, FUNC(z88_state::bank0_cart_w)));
					break;
				case 1:
					m_maincpu->space(AS_PROGRAM).install_readwrite_handler(0x4000, 0x7fff, read8sm_delegate(*this, FUNC(z88_state::bank1_cart_r)), write8sm_delegate(*this, FUNC(z88_state::bank1_cart_w)));
					break;
				case 2:
					m_maincpu->space(AS_PROGRAM).install_readwrite_handler(0x8000, 0xbfff, read8sm_delegate(*this, FUNC(z88_state::bank2_cart_r)), write8sm_delegate(*this, FUNC(z88_state::bank2_cart_w)));
					break;
				case 3:
					m_maincpu->space(AS_PROGRAM).install_readwrite_handler(0xc000, 0xffff, read8sm_delegate(*this, FUNC(z88_state::bank3_cart_r)), write8sm_delegate(*this, FUNC(z88_state::bank3_cart_w)));
					break;
			}

			m_bank_type[bank] = Z88_BANK_CART;
		}
	}


	// override setting for lower 8k of bank 0
	if (bank == 0)
	{
		m_maincpu->space(AS_PROGRAM).install_read_bank(0, 0x1fff, m_banks[0]);

		// enable RAM
		if (rams)
			m_maincpu->space(AS_PROGRAM).install_write_bank(0, 0x1fff, m_banks[0]);
		else
			m_maincpu->space(AS_PROGRAM).unmap_write(0, 0x1fff);

		m_banks[0]->set_entry(rams & 1);
	}
}


void z88_state::z88_mem(address_map &map)
{
	map(0x0000, 0x1fff).bankrw(m_banks[0]);
	map(0x2000, 0x3fff).bankrw(m_banks[1]);
	map(0x4000, 0x7fff).bankrw(m_banks[2]);
	map(0x8000, 0xbfff).bankrw(m_banks[3]);
	map(0xc000, 0xffff).bankrw(m_banks[4]);
}

void z88_state::z88_io(address_map &map)
{
	map(0x0000, 0xffff).rw(m_blink, FUNC(upd65031_device::read), FUNC(upd65031_device::write));
}



/*
-------------------------------------------------------------------------
         | D7     D6      D5      D4      D3      D2      D1      D0
-------------------------------------------------------------------------
A15 (#7) | RSH    SQR     ESC     INDEX   CAPS    .       /       ??
A14 (#6) | HELP   LSH     TAB     DIA     MENU    ,       ;       '
A13 (#5) | [      SPACE   1       Q       A       Z       L       0
A12 (#4) | ]      LFT     2       W       S       X       M       P
A11 (#3) | -      RGT     3       E       D       C       K       9
A10 (#2) | =      DWN     4       R       F       V       J       O
A9  (#1) | \      UP      5       T       G       B       U       I
A8  (#0) | DEL    ENTER   6       Y       H       N       7       8
-------------------------------------------------------------------------

2008-05 FP:
Small note about natural keyboard: currently,
- "Square" is mapped to 'F1'
- "Diamond" is mapped to 'Left Control'
- "Index" is mapped to 'F2'
- "Menu" is mapped to 'F3'
- "Help" is mapped to 'F4'
*/

static INPUT_PORTS_START( z88 )
	PORT_START("LINE0")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Del") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(8)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_ENTER)        PORT_CHAR(13)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)            PORT_CHAR('6') PORT_CHAR('^')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y)            PORT_CHAR('y') PORT_CHAR('Y')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_H)            PORT_CHAR('h') PORT_CHAR('H')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_N)            PORT_CHAR('n') PORT_CHAR('N')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)            PORT_CHAR('7') PORT_CHAR('&')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)            PORT_CHAR('8') PORT_CHAR('*')

	PORT_START("LINE1")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR('\\') PORT_CHAR('|')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_UP)           PORT_CHAR(UCHAR_MAMEKEY(UP))
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)            PORT_CHAR('5') PORT_CHAR('%')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_T)            PORT_CHAR('t') PORT_CHAR('T')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_G)            PORT_CHAR('g') PORT_CHAR('G')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_B)            PORT_CHAR('b') PORT_CHAR('B')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_U)            PORT_CHAR('u') PORT_CHAR('U')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_I)            PORT_CHAR('i') PORT_CHAR('I')

	PORT_START("LINE2")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)       PORT_CHAR('=') PORT_CHAR('+')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_DOWN)     PORT_CHAR(UCHAR_MAMEKEY(DOWN))
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)            PORT_CHAR('4') PORT_CHAR('$')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_R)            PORT_CHAR('r') PORT_CHAR('R')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_F)            PORT_CHAR('f') PORT_CHAR('F')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_V)            PORT_CHAR('v') PORT_CHAR('V')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_J)            PORT_CHAR('j') PORT_CHAR('J')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_O)            PORT_CHAR('o') PORT_CHAR('O')

	PORT_START("LINE3")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)        PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_RIGHT)        PORT_CHAR(UCHAR_MAMEKEY(RIGHT))
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)            PORT_CHAR('3') PORT_CHAR('#')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_E)            PORT_CHAR('e') PORT_CHAR('E')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_D)            PORT_CHAR('d') PORT_CHAR('D')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_C)            PORT_CHAR('c') PORT_CHAR('C')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_K)            PORT_CHAR('k') PORT_CHAR('K')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)            PORT_CHAR('9') PORT_CHAR('(')

	PORT_START("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR(']') PORT_CHAR('}')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_LEFT)     PORT_CHAR(UCHAR_MAMEKEY(LEFT))
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)            PORT_CHAR('2') PORT_CHAR('@')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_W)            PORT_CHAR('w') PORT_CHAR('W')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_S)            PORT_CHAR('s') PORT_CHAR('S')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_X)            PORT_CHAR('x') PORT_CHAR('X')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_M)            PORT_CHAR('m') PORT_CHAR('M')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_P)            PORT_CHAR('p') PORT_CHAR('P')

	PORT_START("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR('[') PORT_CHAR('{')
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SPACE)        PORT_CHAR(' ')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)            PORT_CHAR('1') PORT_CHAR('!')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q)            PORT_CHAR('q') PORT_CHAR('Q')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_A)            PORT_CHAR('a') PORT_CHAR('A')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)            PORT_CHAR('z') PORT_CHAR('Z')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_L)            PORT_CHAR('l') PORT_CHAR('L')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)            PORT_CHAR('0') PORT_CHAR(')')

	PORT_START("LINE6")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Help") PORT_CODE(KEYCODE_F4)             PORT_CHAR(UCHAR_MAMEKEY(F4))
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift (Left)") PORT_CODE(KEYCODE_LSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_TAB)                                  PORT_CHAR('\t')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Diamond") PORT_CODE(KEYCODE_LCONTROL)        PORT_CHAR(UCHAR_SHIFT_2)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Menu") PORT_CODE(KEYCODE_F3)             PORT_CHAR(UCHAR_MAMEKEY(F3))
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)                                PORT_CHAR(',') PORT_CHAR('<')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)                                PORT_CHAR(';') PORT_CHAR(':')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)                                PORT_CHAR('\'') PORT_CHAR('\"')

	PORT_START("LINE7")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift (Right)") PORT_CODE(KEYCODE_RSHIFT)    PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Square") PORT_CODE(KEYCODE_F1)               PORT_CHAR(UCHAR_MAMEKEY(F1))
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_ESC)                              PORT_CHAR(UCHAR_MAMEKEY(ESC))
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Index") PORT_CODE(KEYCODE_F2)                PORT_CHAR(UCHAR_MAMEKEY(F2))
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Caps Lock") PORT_CODE(KEYCODE_CAPSLOCK)      PORT_CHAR(UCHAR_MAMEKEY(CAPSLOCK))
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)                             PORT_CHAR('.') PORT_CHAR('>')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)                                PORT_CHAR('/') PORT_CHAR('?')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)                            PORT_CHAR(0xA3) PORT_CHAR('~')
INPUT_PORTS_END

static INPUT_PORTS_START( z88de )
	PORT_INCLUDE(z88)

	PORT_MODIFY("LINE0")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)            PORT_CHAR('6') PORT_CHAR('&') PORT_CHAR(0x00B0)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)            PORT_CHAR('z') PORT_CHAR('Z')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)            PORT_CHAR('7') PORT_CHAR('/') PORT_CHAR('{')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)            PORT_CHAR('8') PORT_CHAR('(') PORT_CHAR('}')

	PORT_MODIFY("LINE1")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR('<') PORT_CHAR('>')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)            PORT_CHAR('5') PORT_CHAR('%') PORT_CHAR('~')

	PORT_MODIFY("LINE2")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)       PORT_CHAR('\'') PORT_CHAR('`')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)            PORT_CHAR('4') PORT_CHAR('$') PORT_CHAR('|')

	PORT_MODIFY("LINE3")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)        PORT_CHAR(0x00DF) PORT_CHAR('?')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)            PORT_CHAR('3') PORT_CHAR(0x00A7) PORT_CHAR(0x00A3)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)            PORT_CHAR('9') PORT_CHAR(')') PORT_CHAR('[')

	PORT_MODIFY("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR('+') PORT_CHAR('*')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)            PORT_CHAR('2') PORT_CHAR('\"') PORT_CHAR('@')

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR(0x00FC) PORT_CHAR(0x00DC)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)            PORT_CHAR('1') PORT_CHAR('!') PORT_CHAR('\\')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y)            PORT_CHAR('y') PORT_CHAR('Y')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)            PORT_CHAR('0') PORT_CHAR('=') PORT_CHAR(']')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)        PORT_CHAR(',') PORT_CHAR(';')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(0x00E4) PORT_CHAR(0x00C4)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR(0x00F6) PORT_CHAR(0x00D6)

	PORT_MODIFY("LINE7")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)         PORT_CHAR('.') PORT_CHAR(':')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)        PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)    PORT_CHAR('#') PORT_CHAR('^')
INPUT_PORTS_END

static INPUT_PORTS_START( z88fr )
	PORT_INCLUDE(z88)

	PORT_MODIFY("LINE0")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)            PORT_CHAR(0x00A7) PORT_CHAR('6') PORT_CHAR('^')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)            PORT_CHAR(0x00E8) PORT_CHAR('7') PORT_CHAR('{')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)            PORT_CHAR('!') PORT_CHAR('8') PORT_CHAR('}')

	PORT_MODIFY("LINE1")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)    PORT_CHAR('<') PORT_CHAR('>')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)            PORT_CHAR('(') PORT_CHAR('5') PORT_CHAR('~')

	PORT_MODIFY("LINE2")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)        PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)            PORT_CHAR('\'') PORT_CHAR('4') PORT_CHAR('|')

	PORT_MODIFY("LINE3")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)       PORT_CHAR(')') PORT_CHAR(0x00B0)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)            PORT_CHAR('\"') PORT_CHAR('3') PORT_CHAR('#')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)            PORT_CHAR(0x00E7) PORT_CHAR('9') PORT_CHAR('[')

	PORT_MODIFY("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR('=') PORT_CHAR('+')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)            PORT_CHAR(0x00E9) PORT_CHAR('2') PORT_CHAR('@')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)            PORT_CHAR('z') PORT_CHAR('Z')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)        PORT_CHAR(',') PORT_CHAR('?')

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR('*') PORT_CHAR(0x00A8)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)            PORT_CHAR('&') PORT_CHAR('1') PORT_CHAR('\\')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_A)            PORT_CHAR('a') PORT_CHAR('A')
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Q)            PORT_CHAR('q') PORT_CHAR('Q')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_W)            PORT_CHAR('w') PORT_CHAR('W')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)            PORT_CHAR(0x00E0) PORT_CHAR('0') PORT_CHAR(']')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(';') PORT_CHAR('.')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_M)            PORT_CHAR('m') PORT_CHAR('M')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR(0x00F9) PORT_CHAR('%')

	PORT_MODIFY("LINE7")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)         PORT_CHAR(':') PORT_CHAR('/')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)        PORT_CHAR('$') PORT_CHAR(0x00A3)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)
INPUT_PORTS_END

static INPUT_PORTS_START( z88es )
	PORT_INCLUDE(z88)

	PORT_MODIFY("LINE0")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)            PORT_CHAR('6') PORT_CHAR('/') PORT_CHAR('^')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)            PORT_CHAR('7') PORT_CHAR('&') PORT_CHAR('{')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)            PORT_CHAR('8') PORT_CHAR('*') PORT_CHAR('}')

	PORT_MODIFY("LINE1")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)            PORT_CHAR('5') PORT_CHAR('%') PORT_CHAR('~')
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR(0x00E7) PORT_CHAR(0x00C7)

	PORT_MODIFY("LINE2")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)            PORT_CHAR('4') PORT_CHAR('$') PORT_CHAR('|')

	PORT_MODIFY("LINE3")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)            PORT_CHAR('3') PORT_CHAR('#') PORT_CHAR(0x00A3)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)            PORT_CHAR('9') PORT_CHAR('(') PORT_CHAR('[')

	PORT_MODIFY("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)            PORT_CHAR('2') PORT_CHAR(0x00BF) PORT_CHAR('@')

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)            PORT_CHAR('1') PORT_CHAR(0x00A1) PORT_CHAR('\\')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)            PORT_CHAR('0') PORT_CHAR(')') PORT_CHAR(']')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)        PORT_CHAR(',') PORT_CHAR('?')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)        PORT_CHAR(0x00F1) PORT_CHAR(0x00D1)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(';') PORT_CHAR(':')

	PORT_MODIFY("LINE7")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)         PORT_CHAR('.') PORT_CHAR('!')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR('\'') PORT_CHAR('\"')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)    PORT_CHAR('<') PORT_CHAR('>')
INPUT_PORTS_END

static INPUT_PORTS_START( z88it )
	PORT_INCLUDE(z88)

	PORT_MODIFY("LINE0")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)            PORT_CHAR('6') PORT_CHAR('^')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)            PORT_CHAR('7') PORT_CHAR('&')

	PORT_MODIFY("LINE1")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR(0x00F9) PORT_CHAR('|') PORT_CHAR('\\')

	PORT_MODIFY("LINE3")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)            PORT_CHAR('9') PORT_CHAR('(') PORT_CHAR('{')

	PORT_MODIFY("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR(0x00EC) PORT_CHAR('~') PORT_CHAR(']')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)            PORT_CHAR('2') PORT_CHAR(0x00A3)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)            PORT_CHAR('z') PORT_CHAR('Z')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(';') PORT_CHAR(':')

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR(0x00E8) PORT_CHAR(0x00E9) PORT_CHAR('[')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_W)            PORT_CHAR('w') PORT_CHAR('W')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)            PORT_CHAR('0') PORT_CHAR(')') PORT_CHAR('}')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_M)            PORT_CHAR('m') PORT_CHAR('M')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR(0x00F2) PORT_CHAR('\'') PORT_CHAR('@')

	PORT_MODIFY("LINE7")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)    PORT_CHAR(0x00E0) PORT_CHAR('\"') PORT_CHAR('`')
INPUT_PORTS_END

static INPUT_PORTS_START( z88se )
	PORT_INCLUDE(z88)

	PORT_MODIFY("LINE1")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)        PORT_CHAR('/') PORT_CHAR('?')

	PORT_MODIFY("LINE2")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR('+') PORT_CHAR('>')

	PORT_MODIFY("LINE3")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)       PORT_CHAR('=') PORT_CHAR('<')

	PORT_MODIFY("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR('\'') PORT_CHAR('\"') PORT_CHAR('`')

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(0x00E5) PORT_CHAR(0x00C5) PORT_CHAR('\\')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)        PORT_CHAR(',') PORT_CHAR(';')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR(0x00F6) PORT_CHAR(0x00D6) PORT_CHAR('{')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR(0x00E4) PORT_CHAR(0x00C4) PORT_CHAR('}')

	PORT_MODIFY("LINE7")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)         PORT_CHAR('.') PORT_CHAR(':')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)        PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)    PORT_CHAR(0x00A3) PORT_CHAR('~')
INPUT_PORTS_END

static INPUT_PORTS_START( z88no )
	PORT_INCLUDE(z88se)

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(0x00E5) PORT_CHAR(0x00C5) PORT_CHAR('}')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR(0x00E6) PORT_CHAR(0x00C6) PORT_CHAR('{')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR(0x00F8) PORT_CHAR(0x00D8) PORT_CHAR('|')
INPUT_PORTS_END

static INPUT_PORTS_START( z88ch )
	PORT_INCLUDE(z88)

	PORT_MODIFY("LINE0")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)            PORT_CHAR('6') PORT_CHAR('&') PORT_CHAR('^')
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Z)            PORT_CHAR('z') PORT_CHAR('Z')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)            PORT_CHAR('7') PORT_CHAR('/') PORT_CHAR('{')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)            PORT_CHAR('8') PORT_CHAR('(') PORT_CHAR('[')

	PORT_MODIFY("LINE1")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR('<') PORT_CHAR('>')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)            PORT_CHAR('5') PORT_CHAR('%') PORT_CHAR('~')

	PORT_MODIFY("LINE2")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)       PORT_CHAR(0x00FA) PORT_CHAR(0x00F9) PORT_CHAR('`')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)            PORT_CHAR('4') PORT_CHAR(0x00E7) PORT_CHAR(0x00C7)

	PORT_MODIFY("LINE3")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR('\'') PORT_CHAR('?') PORT_CHAR('|')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)            PORT_CHAR('3') PORT_CHAR('*') PORT_CHAR('#')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)            PORT_CHAR('9') PORT_CHAR(')') PORT_CHAR(']')

	PORT_MODIFY("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR(0x00A8) PORT_CHAR('!')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)            PORT_CHAR('2') PORT_CHAR('\"') PORT_CHAR('@')

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR(0x00E8) PORT_CHAR(0x00FC)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)            PORT_CHAR('1') PORT_CHAR('+') PORT_CHAR('\\')
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_Y)            PORT_CHAR('y') PORT_CHAR('Y')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)            PORT_CHAR('0') PORT_CHAR('=') PORT_CHAR('}')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)        PORT_CHAR(',') PORT_CHAR(';')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(0x00E9) PORT_CHAR(0x00F6)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)        PORT_CHAR(0x00E0) PORT_CHAR(0x00E4)

	PORT_MODIFY("LINE7")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)         PORT_CHAR('.') PORT_CHAR(':')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)        PORT_CHAR('-') PORT_CHAR('_')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)    PORT_CHAR('$') PORT_CHAR(0x00A3)
INPUT_PORTS_END

static INPUT_PORTS_START( z88tr )
	PORT_INCLUDE(z88)

	PORT_MODIFY("LINE0")
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_6)            PORT_CHAR('6') PORT_CHAR('?') PORT_CHAR('>')
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_7)            PORT_CHAR('7') PORT_CHAR('/') PORT_CHAR('{')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_8)            PORT_CHAR('8') PORT_CHAR('*') PORT_CHAR('}')

	PORT_MODIFY("LINE1")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_QUOTE)        PORT_CHAR('\'') PORT_CHAR('\"') PORT_CHAR('`')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_5)            PORT_CHAR('5') PORT_CHAR('%') PORT_CHAR('<')

	PORT_MODIFY("LINE2")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_EQUALS)       PORT_CHAR('=') PORT_CHAR('+') PORT_CHAR('^')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_4)            PORT_CHAR('4') PORT_CHAR('$') PORT_CHAR('~')

	PORT_MODIFY("LINE3")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_MINUS)        PORT_CHAR('-') PORT_CHAR('_') PORT_CHAR('\\')
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_3)            PORT_CHAR('3') PORT_CHAR('#') PORT_CHAR(0x00A3)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_9)            PORT_CHAR('9') PORT_CHAR('(') PORT_CHAR('[')

	PORT_MODIFY("LINE4")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_CLOSEBRACE)   PORT_CHAR(0x00FC) PORT_CHAR(0x00DC)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_2)            PORT_CHAR('2') PORT_CHAR('&') PORT_CHAR('@')

	PORT_MODIFY("LINE5")
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_OPENBRACE)    PORT_CHAR(0x011F) PORT_CHAR(0x011E)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_1)            PORT_CHAR('1') PORT_CHAR('!') PORT_CHAR('|')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_0)            PORT_CHAR('0') PORT_CHAR(')') PORT_CHAR(']')

	PORT_MODIFY("LINE6")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH)    PORT_CHAR(0x00F6) PORT_CHAR(0x00D6)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COLON)        PORT_CHAR(0x015F) PORT_CHAR(0x015E)
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_BACKSLASH2)   PORT_CHAR(0x0131) PORT_CHAR(0x0130)

	PORT_MODIFY("LINE7")
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_SLASH)        PORT_CHAR(0x00E7) PORT_CHAR(0x00C7)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_STOP)         PORT_CHAR('.') PORT_CHAR(':')
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_CODE(KEYCODE_COMMA)        PORT_CHAR(',') PORT_CHAR(';')
INPUT_PORTS_END


void z88_state::machine_start()
{
	m_bios = (uint8_t*)m_bios_region->base();
	m_ram_base = (uint8_t*)m_ram->pointer();

	// configure the memory banks

	m_banks[0]->configure_entry(0, m_bios);
	m_banks[0]->configure_entry(1, m_ram_base);
	m_banks[1]->configure_entries(0, 32, m_bios, 0x4000);
	m_banks[2]->configure_entries(0, 32, m_bios, 0x4000);
	m_banks[3]->configure_entries(0, 32, m_bios, 0x4000);
	m_banks[4]->configure_entries(0, 32, m_bios, 0x4000);
	m_banks[1]->configure_entries(32, m_ram->size()>>14, m_ram_base, 0x4000);
	m_banks[2]->configure_entries(32, m_ram->size()>>14, m_ram_base, 0x4000);
	m_banks[3]->configure_entries(32, m_ram->size()>>14, m_ram_base, 0x4000);
	m_banks[4]->configure_entries(32, m_ram->size()>>14, m_ram_base, 0x4000);
}

void z88_state::machine_reset()
{
	m_bank_type[0] = m_bank_type[1] = m_bank_type[2] = m_bank_type[3] = 0;
}

uint8_t z88_state::kb_r(offs_t offset)
{
	uint8_t data = 0xff;

	for (int i = 7; i >= 0; i--)
	{
		if (!BIT(offset, i))
			data &= m_lines[i]->read();
	}

	return data;
}

static void z88_cart(device_slot_interface &device)
{
	device.option_add("32krom",     Z88_32K_ROM);       // 32KB ROM cart
	device.option_add("128krom",    Z88_128K_ROM);      // 128KB ROM cart
	device.option_add("256krom",    Z88_256K_ROM);      // 256KB ROM cart
	device.option_add("32kram",     Z88_32K_RAM);       // 32KB RAM cart
	device.option_add("128kram",    Z88_128K_RAM);      // 128KB RAM cart
	device.option_add("512kram",    Z88_512K_RAM);      // 512KB RAM cart
	device.option_add("1024kram",   Z88_1024K_RAM);     // 1024KB RAM cart
	device.option_add("1024kflash", Z88_1024K_FLASH);   // 1024KB Flash cart
}

void z88_state::z88(machine_config &config)
{
	/* basic machine hardware */
	Z80(config, m_maincpu, XTAL(9'830'400)/3);  // divided by 3 through the uPD65031
	m_maincpu->set_addrmap(AS_PROGRAM, &z88_state::z88_mem);
	m_maincpu->set_addrmap(AS_IO, &z88_state::z88_io);

	/* video hardware */
	SCREEN(config, m_screen, SCREEN_TYPE_LCD);
	m_screen->set_refresh_hz(50);
	m_screen->set_vblank_time(ATTOSECONDS_IN_USEC(0));
	m_screen->set_size(Z88_SCREEN_WIDTH, Z88_SCREEN_HEIGHT);
	m_screen->set_visarea(0, (Z88_SCREEN_WIDTH - 1), 0, (Z88_SCREEN_HEIGHT - 1));
	m_screen->set_palette(m_palette);
	m_screen->set_screen_update("blink", FUNC(upd65031_device::screen_update));

	PALETTE(config, m_palette, FUNC(z88_state::z88_palette), Z88_NUM_COLOURS);

	UPD65031(config, m_blink, XTAL(9'830'400));
	m_blink->kb_rd_callback().set(FUNC(z88_state::kb_r));
	m_blink->int_wr_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ0);
	m_blink->nmi_wr_callback().set_inputline(m_maincpu, INPUT_LINE_NMI);
	m_blink->spkr_wr_callback().set("speaker", FUNC(speaker_sound_device::level_w));
	m_blink->set_screen_update_callback(FUNC(z88_state::lcd_update));
	m_blink->set_memory_update_callback(FUNC(z88_state::bankswitch_update));

	/* sound hardware */
	SPEAKER(config, "mono").front_center();
	SPEAKER_SOUND(config, "speaker").add_route(ALL_OUTPUTS, "mono", 0.50);

	/* internal ram */
	RAM(config, RAM_TAG).set_default_size("128K").set_extra_options("32K,64K,256K,512K");

	/* cartridges */
	Z88CART_SLOT(config, m_carts[1], z88_cart, nullptr);
	m_carts[1]->out_flp_callback().set(m_blink, FUNC(upd65031_device::flp_w));

	Z88CART_SLOT(config, m_carts[2], z88_cart, nullptr);
	m_carts[2]->out_flp_callback().set(m_blink, FUNC(upd65031_device::flp_w));

	Z88CART_SLOT(config, m_carts[3], z88_cart, nullptr);
	m_carts[3]->out_flp_callback().set(m_blink, FUNC(upd65031_device::flp_w));

	/* software lists */
	SOFTWARE_LIST(config, "cart_list").set_original("z88_cart");
}


/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START(z88)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_DEFAULT_BIOS("v40uk")
	ROM_SYSTEM_BIOS( 0, "v220uk", "Version 2.20 UK")
	ROMX_LOAD("z88v220.rom", 0x00000, 0x20000, CRC(0ae7d0fc) SHA1(5d89e8d98d2cc0acb8cd42dbfca601b7bd09c51e), ROM_BIOS(0) )
	ROM_SYSTEM_BIOS( 1, "v30uk", "Version 3.0 UK")
	ROMX_LOAD("z88v300.rom" ,  0x00000, 0x20000, CRC(802cb9aa) SHA1(ceb688025b79454cf229cae4dbd0449df2747f79), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 2, "v40uk", "Version 4.0 UK")
	ROMX_LOAD("z88v400.rom",   0x00000, 0x20000, CRC(1356d440) SHA1(23c63ceced72d0a9031cba08d2ebc72ca336921d), ROM_BIOS(2) )
ROM_END

ROM_START(z88de)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v318de", "Version 3.18 German")
	ROMX_LOAD("z88v318de.rom", 0x00000, 0x20000, CRC(d7eaf937) SHA1(5acbfa324e2a6582ffd1af5f2e28086318d2ed27), ROM_BIOS(0) )
ROM_END

ROM_START(z88es)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v319es", "Version 3.19 Spanish")
	ROMX_LOAD("z88v319es.rom", 0x00000, 0x20000, CRC(7a08af73) SHA1(a99a7581f47a032e1ec3b4f534c06f00f67647df), ROM_BIOS(0) )
ROM_END

ROM_START(z88fr)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v326fr", "Version 3.26 French")
	ROMX_LOAD("z88v326fr.rom", 0x00000, 0x20000, CRC(218fbb72) SHA1(6e4c590f40f5b14d66e6559807f538fb5fa91474), ROM_BIOS(0) )
ROM_END

ROM_START(z88it)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v323it", "Version 3.23 Italian")
	ROMX_LOAD("z88v323it.rom", 0x00000, 0x20000, CRC(13f54308) SHA1(29bda04ae803f2dff6357d81b3894db669d12dbf), ROM_BIOS(0) )
ROM_END

ROM_START(z88se)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v250se", "Version 2.50 Swedish")
	ROMX_LOAD("z88v250sw.rom", 0x00000, 0x20000, CRC(dad01338) SHA1(3825eee346b692b16215a500250cc0c76d2d8f0b), ROM_BIOS(0) )
ROM_END

ROM_START(z88fi)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v401fi", "Version 4.01 Finnish")
	ROMX_LOAD("z88v401fi.rom", 0x00000, 0x20000, CRC(ecd7f3f6) SHA1(bf8d3e083f1959e5a0d7e9c8d2ad0c14abd46381), ROM_BIOS(0) )
ROM_END

ROM_START(z88no)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v260no", "Version 2.60 Norwegian")
	ROMX_LOAD("z88v260nr.rom", 0x00000, 0x20000, CRC(293f35c8) SHA1(b68b8f5bb1f69fe7a24897933b1464dc79e96d80), ROM_BIOS(0) )
ROM_END

ROM_START(z88dk)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v321dk", "Version 3.21 Danish")
	ROMX_LOAD("z88v321dk.rom", 0x00000, 0x20000, CRC(baa80408) SHA1(7b0d44af2688d0fe47667e0424860aafa0948dae), ROM_BIOS(0) )
ROM_END

ROM_START(z88ch)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v313ch", "Version 3.13 Swiss")
	ROMX_LOAD("z88v313he.rom", 0x00000, 0x20000, CRC(a56d732c) SHA1(c2276a12d457f01a8fd2e2ac238aff2b5c3559d8), ROM_BIOS(0) )
ROM_END

ROM_START(z88tr)
	ROM_REGION(0x80000, "bios", ROMREGION_ERASEFF)
	ROM_SYSTEM_BIOS( 0, "v317tk", "Version 3.17 Turkish")
	ROMX_LOAD("z88v317tk.rom", 0x00000, 0x20000, CRC(9468d677) SHA1(8d76e94f43846c736bf257d15d531c2df1e20fae), ROM_BIOS(0) )
ROM_END

/*    YEAR  NAME   PARENT  COMPAT  MACHINE  INPUT  CLASS      INIT        COMPANY                FULLNAME           FLAGS */
COMP( 1988, z88,   0,      0,      z88,     z88,   z88_state, empty_init, "Cambridge Computers", "Z88"            , MACHINE_NOT_WORKING)
COMP( 1988, z88de, z88,    0,      z88,     z88de, z88_state, empty_init, "Cambridge Computers", "Z88 (German)"   , MACHINE_NOT_WORKING)
COMP( 1988, z88es, z88,    0,      z88,     z88es, z88_state, empty_init, "Cambridge Computers", "Z88 (Spanish)"  , MACHINE_NOT_WORKING)
COMP( 1988, z88fr, z88,    0,      z88,     z88fr, z88_state, empty_init, "Cambridge Computers", "Z88 (French)"   , MACHINE_NOT_WORKING)
COMP( 1988, z88it, z88,    0,      z88,     z88it, z88_state, empty_init, "Cambridge Computers", "Z88 (Italian)"  , MACHINE_NOT_WORKING)
COMP( 1988, z88se, z88,    0,      z88,     z88se, z88_state, empty_init, "Cambridge Computers", "Z88 (Swedish)"  , MACHINE_NOT_WORKING)
COMP( 1988, z88fi, z88,    0,      z88,     z88se, z88_state, empty_init, "Cambridge Computers", "Z88 (Finnish)"  , MACHINE_NOT_WORKING)
COMP( 1988, z88no, z88,    0,      z88,     z88no, z88_state, empty_init, "Cambridge Computers", "Z88 (Norwegian)", MACHINE_NOT_WORKING)
COMP( 1988, z88dk, z88,    0,      z88,     z88no, z88_state, empty_init, "Cambridge Computers", "Z88 (Danish)"   , MACHINE_NOT_WORKING)
COMP( 1988, z88ch, z88,    0,      z88,     z88ch, z88_state, empty_init, "Cambridge Computers", "Z88 (Swiss)"    , MACHINE_NOT_WORKING)
COMP( 1988, z88tr, z88,    0,      z88,     z88tr, z88_state, empty_init, "Cambridge Computers", "Z88 (Turkish)"  , MACHINE_NOT_WORKING)
