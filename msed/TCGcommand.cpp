/* C:B**************************************************************************
This software is Copyright 2014 Michael Romeo <r0m30@r0m30.com>

This file is part of msed.

msed is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

msed is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with msed.  If not, see <http://www.gnu.org/licenses/>.

* C:E********************************************************************** */
#include "os.h"
#include <stdio.h>
#include "TCGcommand.h"
#include "TCGdev.h"
#include "endianfixup.h"
#include "hexDump.h"
#include "TCGstructures.h"

/*
 * Initialize: allocate the buffers *ONLY*
 * reset needs to be called to
 * initialize the headers etc
 */
TCGcommand::TCGcommand()
{
	LOG(D4) << "Creating TCGcommand()";
    cmdbuf = (uint8_t *) ALIGNED_ALLOC(4096, IO_BUFFER_LENGTH);
	respbuf = (uint8_t *) ALIGNED_ALLOC(4096, IO_BUFFER_LENGTH);
}

/* Fill in the header information and format the call */
TCGcommand::TCGcommand(TCG_UID InvokingUid, TCG_METHOD method)
{
	LOG(D4) << "Creating TCGvommand(uint16_t ID, TCG_UID InvokingUid, TCG_METHOD method)";
    /* allocate the cmdbuf */
    cmdbuf = (uint8_t *) ALIGNED_ALLOC(4096, IO_BUFFER_LENGTH);
    reset(InvokingUid, method);
}

/* Fill in the header information ONLY (no call) */
void
TCGcommand::reset()
{
	LOG(D4) << "Entering TCGcommand::reset(uint16_t comID)";
    memset(cmdbuf, 0, IO_BUFFER_LENGTH);
    TCGHeader * hdr;
    hdr = (TCGHeader *) cmdbuf;

    bufferpos = sizeof (TCGHeader);
}

void
TCGcommand::reset(TCG_UID InvokingUid, TCG_METHOD method)
{
	LOG(D4) << "Entering TCGcommand::reset(uint16_t comID, TCG_UID InvokingUid, TCG_METHOD method)";
    reset(); // build the headers
    cmdbuf[bufferpos++] = TCG_TOKEN::CALL;
    cmdbuf[bufferpos++] = TCG_SHORT_ATOM::BYTESTRING8;
    memcpy(&cmdbuf[bufferpos], &TCGUID[InvokingUid][0], 8); /* bytes 2-9 */
    bufferpos += 8;
    cmdbuf[bufferpos++] = TCG_SHORT_ATOM::BYTESTRING8;
    memcpy(&cmdbuf[bufferpos], &TCGMETHOD[method][0], 8); /* bytes 11-18 */
    bufferpos += 8;
}

void
TCGcommand::addToken(uint64_t number)
{
	int startat = 0;
	LOG(D4) << "Entering TCGcommand::addToken(uint64_t number)";
	//cmdbuf[bufferpos++] = 0x82;
	//cmdbuf[bufferpos++] = ((number & 0xff00) >> 8);
	//cmdbuf[bufferpos++] = (number & 0x00ff);
	if (number < 64) {
		cmdbuf[bufferpos++] = (uint8_t)number & 0x000000000000003f;
	}
	else
	{
		if (number < 0x100)
		{
			cmdbuf[bufferpos++] = 0x81;
			startat = 0;
		}
		else if (number < 0x10000) {
			cmdbuf[bufferpos++] = 0x82;
			startat = 1;
		}
		else if (number < 0x100000000) {
			cmdbuf[bufferpos++] = 0x84;
			startat = 3;
		}
		else
		{
			cmdbuf[bufferpos++] = 0x88;
			startat = 7;
		}
		for (int i = startat; i > -1; i--) {
			cmdbuf[bufferpos++] = (uint8_t)((number >> (i * 8)) & 0x00000000000000ff);
		}
	}
}
void
TCGcommand::addToken(std::vector<uint8_t> token) {
	LOG(D4) << "Entering addToken(std::vector<uint8_t>)";
	for (uint32_t i = 0; i < token.size(); i++) {
		cmdbuf[bufferpos++] = token[i];
	}
}
void
TCGcommand::addToken(const char * bytestring)
{
	LOG(D4) << "Entering TCGcommand::addToken(const char * bytestring)";
	uint16_t length = (uint16_t) strlen(bytestring);
    if (strlen(bytestring) < 16) {
        /* use tiny atom */
        cmdbuf[bufferpos++] = (uint8_t) length | 0xa0;
    }
    else if(length < 2048) {
        /* Use Medium Atom */
        cmdbuf[bufferpos++] = 0xd0 | (uint8_t) ((length >> 8) & 0x07);
        cmdbuf[bufferpos++] = (uint8_t) (length & 0x00ff);
    }
    else {
        /* Use Large Atom */
        LOG(E) << "FAIL -- can't send LARGE ATOM size bytestring in 2048 Packet";
    }
    memcpy(&cmdbuf[bufferpos], bytestring, (strlen(bytestring)));
    bufferpos += (strlen(bytestring));

}

void
TCGcommand::addToken(TCG_TOKEN token)
{
	LOG(D4) << "Entering TCGcommand::addToken(TCG_TOKEN token)";
	cmdbuf[bufferpos++] = (uint8_t) token;
}

void
TCGcommand::addToken(TCG_TINY_ATOM token)
{
	LOG(D4) << "Entering TCGcommand::addToken(TCG_TINY_ATOM token)";
	cmdbuf[bufferpos++] = (uint8_t) token;
}

void
TCGcommand::addToken(TCG_UID token)
{
	LOG(D4) << "Entering TCGcommand::addToken(TCG_UID token)";
	cmdbuf[bufferpos++] = TCG_SHORT_ATOM::BYTESTRING8;
    memcpy(&cmdbuf[bufferpos], &TCGUID[token][0], 8);
    bufferpos += 8;
}

void
TCGcommand::complete(uint8_t EOD)
{
	LOG(D4) << "Entering TCGcommand::complete(uint8_t EOD)";
    if (EOD) {
        cmdbuf[bufferpos++] = TCG_TOKEN::ENDOFDATA;
        cmdbuf[bufferpos++] = TCG_TOKEN::STARTLIST;
        cmdbuf[bufferpos++] = 0x00;
        cmdbuf[bufferpos++] = 0x00;
        cmdbuf[bufferpos++] = 0x00;
        cmdbuf[bufferpos++] = TCG_TOKEN::ENDLIST;
    }
    /* fill in the lengths and add the modulo 4 padding */
    TCGHeader * hdr;
    hdr = (TCGHeader *) cmdbuf;
    hdr->subpkt.length = SWAP32(bufferpos - sizeof (TCGHeader));
    while (bufferpos % 4 != 0) {
        cmdbuf[bufferpos++] = 0x00;
    }
    hdr->pkt.length = SWAP32((bufferpos - sizeof (TCGComPacket))
                             - sizeof (TCGPacket));
    hdr->cp.length = SWAP32(bufferpos - sizeof (TCGComPacket));
}
void
TCGcommand::changeInvokingUid(std::vector<uint8_t> Invoker)
{
	LOG(D4) << "Entering TCGcommand::changeInvokingUid()";
	int offset = sizeof(TCGHeader) + 1;  /* bytes 2-9 */
	for (uint32_t i = 0; i < Invoker.size(); i++) {
		cmdbuf[offset + i] = Invoker[i];
	}

}
void *
TCGcommand::getCmdBuffer()
{
	return cmdbuf;
}
void *
TCGcommand::getRespBuffer()
{
	return respbuf;
}

void
TCGcommand::setcomID(uint16_t comID)
{
	TCGHeader * hdr;
	hdr = (TCGHeader *)cmdbuf;
	LOG(D4) << "Entering TCGcommand::setcomID()";
	hdr->cp.extendedComID[0] = ((comID & 0xff00) >> 8);
	hdr->cp.extendedComID[1] = (comID & 0x00ff);
	hdr->cp.extendedComID[2] = 0x00;
	hdr->cp.extendedComID[3] = 0x00;
}

void 
TCGcommand::setTSN(uint32_t TSN) {
	TCGHeader * hdr;
	hdr = (TCGHeader *)cmdbuf;
	LOG(D4) << "Entering TCGcommand::setTSN()";
	hdr->pkt.TSN = TSN;
}

void 
TCGcommand::setHSN(uint32_t HSN) {
	TCGHeader * hdr;
	hdr = (TCGHeader *)cmdbuf;
	LOG(D4) << "Entering TCGcommand::setHSN()";
	hdr->pkt.HSN = HSN;
}

TCGcommand::~TCGcommand()
{
	LOG(D4) << "Destroying TCGcommand";
    ALIGNED_FREE(cmdbuf);
	ALIGNED_FREE(respbuf);
}