//
// z80ports.cpp
//
// Copyright (C) 2016-2018  R. Stange <rsta2@o2online.de>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
#include "z80ports.h"
#include "z80computer.h"
#include "z80stub.h"
#include "config.h"
#include <assert.h>

enum TPortAddress
{
	// Console
	PortConsoleStatus = 0x00,	// In
#define PORT_CONSOLE_EMPTY	0x00
#define PORT_CONSOLE_FULL	0x01
	PortConsoleInput,		// In
	PortConsoleOutput,		// Out

	// Disk
	PortDiskTrack	 = 0x10,	// Out
	PortDiskSector,			// Out
	PortDiskDMALow,			// Out
	PortDiskDMAHigh,		// Out
	PortDiskOperation,		// Out
#define PORT_DISK_READ		0x01
#define PORT_DISK_WRITE		0x02
	PortDiskStatus,			// In
#define PORT_DISK_STATUS_OK	0x00
#define PORT_DISK_STATUS_ERROR	0x01
	PortDiskCount,			// In
	PortDiskDrive,			// Out

	// Control
	PortControl	 = 0xE0		// Out
#define PORT_CONTROL_SAVE	'S'
#define PORT_CONTROL_QUIT	'Q'
};

CZ80Ports *CZ80Ports::s_pThis = 0;

CZ80Ports::CZ80Ports (CZ80Computer *pComputer, CZ80Memory *pMemory, CConsole *pConsole,
		      CRAMDisk *pRAMDisk0, CRAMDisk *pRAMDisk1)
:	m_pComputer (pComputer),
	m_pMemory (pMemory),
	m_pConsole (pConsole),
	m_pRAMDisk0 (pRAMDisk0),
	m_pRAMDisk1 (pRAMDisk1),
	m_ucDiskDriveCount (1),
	m_ucDiskDrive (0),
	m_ucDiskTrack (0),
	m_ucDiskSector (0),
	m_usDMAAddress (0x80),
	m_bDiskStatus (FALSE)
{
	assert (s_pThis == 0);
	s_pThis = this;
}

CZ80Ports::~CZ80Ports (void)
{
	s_pThis = 0;
}

boolean CZ80Ports::Initialize (void)
{
	assert (m_pRAMDisk1 != 0);
	m_ucDiskDriveCount = m_pRAMDisk1->IsAvailable () ? 2 : 1;

	return TRUE;
}

u8 CZ80Ports::PortInput (u16 usPort)
{
	usPort &= 0xFF;

	switch (usPort)
	{
	case PortConsoleStatus:
		assert (m_pConsole != 0);
		return m_pConsole->GetStatus () ? PORT_CONSOLE_FULL : PORT_CONSOLE_EMPTY;

	case PortConsoleInput:
		assert (m_pConsole != 0);
		return m_pConsole->GetChar ();

	case PortDiskStatus:
		return m_bDiskStatus ? PORT_DISK_STATUS_OK : PORT_DISK_STATUS_ERROR;

	case PortDiskCount:
		return m_ucDiskDriveCount;

	default:
		break;
	}

	return 0xFF;
}

void CZ80Ports::PortOutput (u16 usPort, u8 ucValue)
{
	usPort &= 0xFF;

	void *pDMABuffer;

	switch (usPort)
	{
	case PortConsoleOutput:
		assert (m_pConsole != 0);
		m_pConsole->PutChar (ucValue);
		break;

	case PortDiskTrack:
		m_ucDiskTrack = ucValue;
		break;

	case PortDiskSector:
		m_ucDiskSector = ucValue;
		break;

	case PortDiskDMALow:
		m_usDMAAddress &= 0xFF00;
		m_usDMAAddress |= ucValue;
		break;

	case PortDiskDMAHigh:
		m_usDMAAddress &= 0x00FF;
		m_usDMAAddress |= ucValue << 8;
		break;

	case PortDiskOperation: {
		m_bDiskStatus = FALSE;

		if (m_ucDiskDrive >= m_ucDiskDriveCount)
		{
			break;
		}

		assert (m_ucDiskDrive <= 1);
		CRAMDisk *pRAMDisk = m_ucDiskDrive == 0 ? m_pRAMDisk0 : m_pRAMDisk1;
		assert (pRAMDisk != 0);

		assert (m_pMemory != 0);
		pDMABuffer = m_pMemory->GetDMAPointer (m_usDMAAddress, SECTOR_SIZE);
		if (pDMABuffer != 0)
		{
			unsigned nSector = SECTORS_PER_TRACK * m_ucDiskTrack + m_ucDiskSector;

			switch (ucValue)
			{
			case PORT_DISK_READ:
				m_bDiskStatus = pRAMDisk->Read (nSector, pDMABuffer);
				break;

			case PORT_DISK_WRITE:
				m_bDiskStatus = pRAMDisk->Write (nSector, pDMABuffer);
				break;

			default:
				break;
			}
		}
		} break;

	case PortDiskDrive:
		m_ucDiskDrive = ucValue;
		break;

	case PortControl:
		switch (ucValue)
		{
		case PORT_CONTROL_SAVE:
			assert (m_pRAMDisk0 != 0);
			m_pRAMDisk0->Save ();

			if (m_pRAMDisk1->IsAvailable ())
			{
				m_pRAMDisk1->Save ();
			}
			break;

		case PORT_CONTROL_QUIT:
			assert (m_pComputer != 0);
			m_pComputer->Shutdown ();
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
}

// Stubs:

unsigned char PortInput (unsigned short usPort)
{
	assert (CZ80Ports::s_pThis != 0);
	return CZ80Ports::s_pThis->PortInput (usPort);
}

void PortOutput (unsigned short usPort, unsigned char ucValue)
{
	assert (CZ80Ports::s_pThis != 0);
	CZ80Ports::s_pThis->PortOutput (usPort, ucValue);
}
