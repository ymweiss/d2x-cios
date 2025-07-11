/*   
	Custom IOS Module (MLOAD)

	Copyright (C) 2008 neimod.
	Copyright (C) 2010 Hermes.
	Copyright (C) 2010 Waninkoko.
	Copyright (C) 2011 davebaol.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _SWI_H_
#define _SWI_H_

#include "types.h"

/* SWI function */
typedef s32 (*SwiFunc)(u32 arg0, u32 arg1, u32 arg2, u32 arg3);


/* Prototypes */
s32 Swi_MLoad(u32 arg0, u32 arg1, u32 arg2, u32 arg3);

/* Externs */
extern SwiFunc SwiTable[];
extern void    SwiVector(void);

int8_t RetreiveHaiParams(u16 id, void* data, void* size);
void dummyCall();

#endif
