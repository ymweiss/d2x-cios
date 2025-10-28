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

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "detect.h"
#include "elf.h"
#include "ios.h"
#include "ipc.h"
#include "mem.h"
#include "module.h"
#include "patches.h"
#include "stealth.h"
#include "swi_mload.h"
#include "syscalls.h"
#include "timer.h"
#include "tools.h"
#include "types.h"
#include "swi.h"

#define DEBUG_MODE DEBUG_NONE
//#define DEBUG_MODE DEBUG_BUFFER
//#define DEBUG_MODE DEBUG_GECKO

// HAI parameter addresses
#define HAI_MAGIC_ADDRESS    0xfffff000
#define HAI_PARAM_SIZE_ADDR  0xfffff004
#define HAI_PARAMS_DATA_ADDR 0xfffff008

//IOS56 pattern to find and replace
static const u16 old_dev_fs_main[] = {
	0xD009,
	0x68A0,
	0xF7FF,
	0x2800,
	0xD104,
	0x1C20,
	0xF7FF,
	0x1C01,
	0xE038,
	0x6823,
	0x2B01,
	0xD009,
	0x68A0,
	0xF7FA,
	0x2800,
	0xD104,
};


static const u16 new_dev_fs_main[] = {
	0xD00c,
	0x6823,
	0xF7FF,
	0x2800,
	0xD103,
	0xE005,
	0xF7FF,
	0x1C01,
	0xE038,
	0x6823,
	0x2B01,
	0xD009,
	0x68A0,
	0xF7FA,
	0x2800,
	0xD1ed,
};



//find and replace directly
static const u16 old_dev_fs_open_flash[] = {
	0xB510,
	0x2005,
	0x4240,
	0x2100,
	0x4C07,
	0x00CB,
	0x191A,
	0x6813,
	0x2B00,
	0xD103,
	0x2301,
	0x6013,
	0x1C10,
	0xE002,
	0x3101,
	0x2901,
	0xD9F3,
	0xBC10,
	0xBC02,
	0x4708,
	0x2004, 
    0x9C44,
	0xB500,
	0x1C02
};

static const u16 new_dev_fs_open_flash[] = {
/* 200051FC */ 0xB500,     // push {lr}
               0x4809,     // ldr r0, =dev_flash_fds
               0x2200,     // movs r2, #0
               0x2B06,     // cmp r3, #6 (ipc_ioctl)
/* 20005204 */ 0xD105,     // bne 20005212
               0x68E1,     // ldr r1, [r4, 0xC] (ipcmsg.ioctl.cmd)
               0x296F,     // cmp r1, #0x6F (111)
/* 2000520A */ 0xD007,     // beq 2000521C
               0x2201,     // movs r2, #1
               0x2964,     // cmp r1, #0x64 (100)
/* 20005210 */ 0xD004,     // beq 2000521C
/* 20005212 */ 0x6800,     // ldr r0, [r0]
               0x2801,     // cmp r0, #1
/* 20005216 */ 0xD103,     // bne 2000521E
/* 20005218 */ 0x4803,     // ldr r0, =_FS_ioctl_ret+1
               0x4700,     // bx r0
/* 2000521C */ 0x6002,     // str r2, [r0]
               0x2001,     // movs r0, #1
/* 20005220 */ 0xBC04,     // pop {r2}
               0x4710,     // bx r2
/* 20005224 */ 0x0000,     // dev_flash_fds  (unmodified)
               0x0000,     //                (unmodified)
/* 20005228 */ 0x1374,     // _FS_ioctl_ret_
               0x001C+1    // _FS_ioctl_ret_+1 (thumb)
};


/* Global variables */
char *moduleName = "MLOAD";
s32 offset       = 0;
u32 stealth_mode = 1;     // Stealth mode is on by default

uint32_t stackBuffer[0x7C]; //use to store the relocated stack arguments count
s32 hai_patch_result = 0; // Store result of HAI patching: 0=success, negative=error
int __PatchSyscalls()
{
	//the target address for the new syscall tables is immediately after the HAI parameters
	const uint16_t* paramAddress = (uint16_t*)HAI_PARAM_SIZE_ADDR;
	uint16_t paramSize = *paramAddress;
	uint8_t* syscallTarget = ((uint8_t*)paramAddress) + paramSize + 4;

	uint32_t stackArgsBase = ios.syscallBase + 0x7A*sizeof(uint32_t);

	memcpy(stackBuffer, (void*)stackArgsBase, 4*0x7A);
	DCWrite32(stackBuffer+0x7A,0);
	DCWrite32(stackBuffer+0x7B,0);
	if (memcmp((void*)stackArgsBase,stackBuffer,4*0x7A) != 0)
	{
		return -1; // Error: stackBuffer memcpy failed
	}
	//the original location of the stack args can be overwritten by the new syscall addresses
	DCWrite32((void*)stackArgsBase,RetreiveHaiParams);
	DCWrite32((void*)(stackArgsBase+4),dummyCall);
	//DCFlushRange(syscallTarget+(0x7A+0x7C)*(sizeof(uint32_t)), 8);
	//patch ios syscall handler to use the new syscall tables and increase the maximum syscall number
	//first find the syscall handler routine
	uint8_t* kernelBase = (uint8_t*)0xffff0000;
	uint8_t pattern[] = {0xE9, 0xCD, 0x7F, 0xFF, 0xE1, 0x4F, 0x80, 0x00};
	uint8_t* index;
	if (ios.syscallBase == 0)
	{
		return -2; // Error: ios.syscallBase is 0
	}

	for (index = kernelBase; index < (uint8_t*)0xffff9000; index+=4)
	{
		if(!memcmp(pattern, index, sizeof(pattern))) //syscall handler found
		{
			//max syscall index is at offset 0x33 from here
			//should be E3 5A 00 7A
			bool syscall_max_patched = false;
			if (*(index+0x33) == 0x7A)
			{
				DCWrite8(index+0x33, 0x7C);
				syscall_max_patched = true;
			}
			
			else
			{
				uint8_t pattern[2] = {0xe3,0x5a};
				uint8_t* address = index;
				for (;address < index+0x100; address++)
				{
					if (!memcmp(pattern,address,2))
					{
						DCWrite8(address+3,0x7C);
						syscall_max_patched = true;
						break;
					}
				}
			}
			if (!syscall_max_patched)
			{
				return -4; // Error: max syscall index patch failed
			}
			//load stack arg counts is at offset 0x48
			offset = (*(uint16_t*)(index+0x4A) & 0x0fff) + 0x8; //lower 16 bits
			bool add = !!(*(uint32_t*)(index+0x48) & 0x02000000); //sign bit in instruction
			uint32_t* stackAddress = (uint32_t*)(index+0x48 - offset + 2*add*offset);
			bool stack_patched = false;
			if (*stackAddress == stackArgsBase)
			{
				DCWrite32(stackAddress, stackBuffer);
				stack_patched = true;
				//*stackAddress = (uint32_t)(syscallTarget+0x7C*sizeof(uint32_t));
			}
			else
			{
				//search the next 0x1000 bytes for the correct offset
				int i;
				for (i = 0; i < 0x400; i++)
				{
					stackAddress = (uint32_t*)(index+0x70)+i;
					if (*stackAddress == stackArgsBase)
					{
						DCWrite32(stackAddress, stackBuffer);
						stack_patched = true;
						//*stackAddress = (uint32_t)(syscallTarget+0x7C*sizeof(uint32_t));
						break;
					}
				}
			}
			
			
			//optionally patch arbitrary syscalls to load a new IOS kernel to a custom handler

			// If we reached here, check if all patches succeeded
			if (!stack_patched)
			{
				return -5; // Error: stack address not found or patch failed
			}

			return 0; // Success - all patches applied
		}
	}

	// If we get here, syscall handler pattern was not found
	return -3; // Error: syscall handler pattern not found
	
}


static s32 __MLoad_Ioctlv(u32 cmd, ioctlv *vector, u32 inlen, u32 iolen)
{
	s32 ret = IPC_ENOENT;

	/* Invalidate cache */
	InvalidateVector(vector, inlen, iolen);

	/* Check command */
	switch (cmd) {
	case MLOAD_GET_IOS_INFO: {
		iosInfo *buffer = vector[0].data;

		/* Copy IOS info */
		memcpy(buffer, &ios, sizeof(ios));

		/* Success */
		ret = 0;

		break;
	}

	case MLOAD_GET_MLOAD_VERSION: {
		/* Return MLOAD version */
		ret = (MLOAD_VER << 4) | MLOAD_SUBVER;

		break;
	}

	case MLOAD_GET_LOAD_BASE: {
		u32 *address = (u32 *)vector[0].data;
		u32 *size    = (u32 *)vector[1].data;

		/* Modules area info */
		*address = (u32)exe_mem;
		*size    = (u32)exe_mem_size;

		/* Success */
		ret = 0;

		break;
	}

	case MLOAD_LOAD_ELF: {
		void *data = vector[0].data;

		/* Load ELF */
		ret = Elf_Load(data);

		break;
	}

	case MLOAD_RUN_ELF: {
		/* Run ELF */
		ret = Elf_Run();

		break;
	}

	case MLOAD_RUN_THREAD: {
		u32 start = *(u32 *)vector[0].data;
		u32 stack = *(u32 *)vector[1].data;
		u32 slen  = *(u32 *)vector[2].data;
		u32 prio  = *(u32 *)vector[3].data;

		/* Run thread */
		ret = Elf_RunThread((void *)start, NULL, (void *)stack, slen, prio);

		break;
	}

	case MLOAD_STOP_THREAD: {
		u32 tid = *(u32 *)vector[0].data;

		/* Stop thread */
		ret = Elf_StopThread(tid);

		break;
	}

	case MLOAD_CONTINUE_THREAD: {
		u32 tid = *(u32 *)vector[0].data;

		/* Continue thread */
		ret = Elf_ContinueThread(tid);

		break;
	}

	case MLOAD_MEMSET: {
		u32 addr = *(u32 *)vector[0].data;
		u32 val  = *(u32 *)vector[1].data;
		u32 len  = *(u32 *)vector[2].data;

		/* Invalidate cache */
		os_sync_before_read((void *)addr, len);

		/* Do memset */
		memset((void *)addr, val, len);

		/* Success */
		ret = 0;

		break;
	}

	case MLOAD_SET_LOG_MODE: {
		u32 mode = *(u32 *)vector[0].data;

		/* Set debug mode */
		ret = Debug_SetMode(mode);

		break;
	}

	case MLOAD_GET_LOG_BUFFER: {
#ifndef NO_DEBUG_BUFFER
		char *buffer = (char *)vector[0].data;
		u32   len    = *(u32 *)vector[0].len;

		/* Get debug buffer */
		ret = Debug_GetBuffer(buffer, len-1);
#else
		ret = 0;
#endif

		break;
	}

	case MLOAD_SET_STEALTH_MODE: {
		u32 mode = *(u32 *)vector[0].data;

		/* Set stealth mode */
		stealth_mode = mode;

		ret = 0;

		break;
	}

	default:
		break;
	}

	/* Flush cache */
	FlushVector(vector, inlen, iolen);

	return ret;
}

static s32 __MLoad_DisableMem2Protection(void)
{
	/* Disable MEM2 protection (so the PPC can access all 64MB) */
	MEM2_Prot(0);

	return 0;
}

/* System detectors and patchers */
static patcher moduleDetectors[] = {
	{Detect_DipModule, 0},
	{Detect_EsModule, 0},
	{Detect_FfsModule, 0},
	{Detect_IopModule, 0},
	{Patch_IopModule, 0}  // We want to patch swi vector asap
};

s32 __MLoad_System(void)
{
	/* Detect modules and patch swi vector */
	s32 result = IOS_PatchModules(moduleDetectors, sizeof(moduleDetectors));
	//apply HAI-IOS patches when HAI parameters are detected
	const uint8_t* HAI_ADDRESS = (uint8_t*)HAI_MAGIC_ADDRESS;
	uint8_t magic[4] = {'H','A','I',0x00};
	if (!memcmp(HAI_ADDRESS,magic,4))
	{
		hai_patch_result = __PatchSyscalls();
	}
	//patch os_load_rm to work with any UID
	uint8_t pattern[10] = {0xf7,0xff,0xf8,0x43,0x28,0x00,0xd0,0x0a,0x1c,0x2e};
	uint8_t* kernelBase = (uint8_t*)0xffff0000;
	for (; kernelBase < (uint8_t*)0xffffa000; kernelBase++)
	{
		if (!memcmp(kernelBase,pattern,10))
		{
			//replace 0xd0 with 0xe0
			DCWrite8(kernelBase+6,0xe0);
			break;
		}
	}
	//include patches that are normally applied by Riivolution
	//patch sd load
	const u8 sd_old[] = {0x22, 0xf4, 0x00, 0x52, 0x18, 0x81, 0x27, 0xf0, 0x00, 0x7f, 0x19, 0xf3, 0x88, 0x0a, 0x88, 0x1b, 0x42, 0x9a};
	for (kernelBase = (uint8_t*)0xffff0000; kernelBase < (uint8_t*)0xffffa000; kernelBase++)
	{
		if (!memcmp(kernelBase, sd_old, sizeof(sd_old)))
		{
			DCWrite16(kernelBase+16,0x2A04);
			break;
		}
	}
	//patch gpio_stm
	static const u8 gpio_orig[8] =  {0xD1, 0x0F, 0x28, 0xFC, 0xD0, 0x33, 0x28, 0xFC};
	static const u8 gpio_orig2[8] = {0xD1, 0x3D, 0x23, 0x89, 0x00, 0x9B, 0x42, 0x98};
	for (kernelBase = (uint8_t*)0xffff0000; kernelBase < (uint8_t*)0xffffa000; kernelBase++)
	{
		if (!memcmp(kernelBase, gpio_orig, sizeof(gpio_orig)) || !memcmp(kernelBase, gpio_orig2, sizeof(gpio_orig2)))
		{
			DCWrite16(kernelBase,0x46C0);
			break;
		}
	}
	//patch prng permissions
	const u16 prng_perms[14] = {0x2005, 0x2103, 0x47A0, 0x2800, 0xD1D3, 0x2006, 0x2103, 0x47A0,
								0x2800, 0xD1CE, 0x200B, 0x2103, 0x47A0, 0x2800};
	for (kernelBase = (uint8_t*)0xffff0000; kernelBase < (uint8_t*)0xffffa000; kernelBase++)
	{
		if (!memcmp(kernelBase, prng_perms, sizeof(prng_perms)))
		{
			DCWrite16(kernelBase+2, ((uint16_t*)kernelBase)[1] | 0x40); // give EHCI pid access to PRNG key
			break;
		}
	}
	//patch fs redirect (not a kernel patch)
	bool replaceMain = false;
	bool replaceFlash = false;
	uint8_t* fsBase;
	for (fsBase = (uint8_t*)0x20000000; fsBase < (uint8_t*)0x20010000; fsBase++)
	{
		if (replaceMain && replaceFlash)
		{
			break;
		}
		if (!replaceMain && !memcmp(fsBase, old_dev_fs_main, sizeof(old_dev_fs_main)))
		{
		    memcpy(fsBase, new_dev_fs_main, sizeof(new_dev_fs_main));
			DCFlushRange(fsBase,sizeof(new_dev_fs_main));
		    replaceMain = true;
		}

		else if (!replaceFlash && !memcmp(fsBase, old_dev_fs_open_flash, sizeof(old_dev_fs_open_flash)))
		{
		    memcpy(fsBase, new_dev_fs_open_flash, sizeof(new_dev_fs_open_flash));
			DCFlushRange(fsBase,sizeof(new_dev_fs_open_flash));
		    replaceFlash = true;
		}
	}
	return result;
}

static s32 __MLoad_Initialize(u32 *queuehandle)
{
	/* Heap space */
	static u32 heapspace[0x1000] ATTRIBUTE_ALIGN(32);

	void *buffer = NULL;
	s32   ret;

	/* Initialize memory heap */
	ret = Mem_Init(heapspace, sizeof(heapspace));
	if (ret < 0)
		return ret;

	/* Initialize timer subsystem */
	ret = Timer_Init();
	if (ret < 0)
		return ret;

	/* Allocate queue buffer */
	buffer = Mem_Alloc(0x20);
	if (!buffer)
		return IPC_ENOMEM;

	/* Create message queue */
	ret = os_message_queue_create(buffer, 8);
	if (ret < 0)
		return ret;

	/* System patchers */
	static patcher patchers[] = {
		{Patch_DipModule, 0},
		{Patch_EsModule, 0},
		{Patch_FfsModule, 0},
		{__MLoad_DisableMem2Protection, 0}
	};

	/* Initialize plugin */
	IOS_InitSystem(patchers, sizeof(patchers));

	/* Register devices */
	os_device_register(DEVICE_NAME, ret);

	/* Copy queue handler */
	*queuehandle = ret;

	return 0;
}



int main(void)
{
	u32 queuehandle;
	s32 ret;

	/* Avoid GCC optimizations */
	exe_mem[0] = 0;

	/* Print info */
	svc_write("$IOSVersion: MLOAD: " __DATE__ " " __TIME__ " 64M$\n");

	/* Call __MLoad_System through software interrupt 9 */
	ret = os_software_IRQ(9);

	/* Set debug mode */
 	Debug_SetMode(DEBUG_MODE);

	/* Check modules patch */
	if (ret) {
		svc_write("MLOAD: ERROR -> Can't detect some IOS modules.\n");
		IOS_CheckPatches(moduleDetectors, sizeof(moduleDetectors));
	}

	/* Initialize module */
	ret = __MLoad_Initialize(&queuehandle);
	if (ret < 0)
		return ret;

	/* Log HAI patch results */
	s32 file = os_open("/cios/log.txt", 0x02);
	s32 test = os_open("/cios/test.txt", 0x02);
	os_close(test);

	char* logMessage = NULL;
	switch (hai_patch_result)
	{
		case 0:
			logMessage = "MLOAD: SUCCESS -> HAI patches applied successfully\n";
			break;
		case -1:
			logMessage = "MLOAD: ERROR -> HAI: stackBuffer memcpy failed\n";
			break;
		case -2:
			logMessage = "MLOAD: ERROR -> HAI: ios.syscallBase is 0\n";
			break;
		case -3:
			logMessage = "MLOAD: ERROR -> HAI: syscall handler pattern not found\n";
			break;
		case -4:
			logMessage = "MLOAD: ERROR -> HAI: max syscall index patch failed\n";
			break;
		case -5:
			logMessage = "MLOAD: ERROR -> HAI: stack address not found or patch failed\n";
			break;
		default:
			// HAI parameters were not detected, no log needed
			break;
	}

	if (logMessage)
	{
		os_write(file, logMessage, strlen(logMessage));
	}

	os_close(file);

	/* Load in the appropriate OH1 module */
	/*
	if (hai_patch_result == 0)
	{
		os_launch_rm("/cios/OH1_HAI.app");
	}
	else
	{
		os_launch_rm("/cios/OH1.app");
	}
	*/
	

	/* Main loop */
	while (1) {
		ipcmessage *message = NULL;

		/* Wait for message */
		ret = os_message_queue_receive(queuehandle, (void *)&message, 0);
		if (ret)
			continue;

		/* Parse command */
		switch (message->command) {
		case IOS_OPEN: {

			/* Block opening request if a title is running */
			ret = Stealth_CheckRunningTitle(NULL);
			if (ret) {
				ret = IPC_ENOENT;
				break;
			}

			/* Check device path */
			if (!strcmp(message->open.device, DEVICE_NAME))
				ret = message->open.resultfd;
			else
				ret = IPC_ENOENT;

			break;
		}

		case IOS_CLOSE: {
			/* Do nothing */
			break;
		}

		case IOS_READ: {
			void *dst = message->read.data;
			void *src = (void *)offset;
			u32   len = message->read.length;

			/* Read data */
			Swi_uMemcpy(dst, src, len);

			/* Update offset */
			offset += len;
		}

		case IOS_WRITE: {
			void *dst = (void *)offset;
			void *src = message->write.data;
			u32   len = message->write.length;

			/* Write data */
			Swi_Memcpy(dst, src, len);

			/* Update offset */
			offset += len;
		}

		case IOS_SEEK: {
			s32 whence = message->seek.origin;
			s32 where  = message->seek.offset;

			/* Update offset */
			switch (whence) {
			case SEEK_SET:
				offset = where;
				break;

			case SEEK_CUR:
				offset += where;
				break;

			case SEEK_END:
				offset = -where;
				break;
			}

			/* Return offset */
			ret = offset;

			break;
		}

		case IOS_IOCTLV: {
			ioctlv *vector = message->ioctlv.vector;
			u32     inlen  = message->ioctlv.num_in;
			u32     iolen  = message->ioctlv.num_io;
			u32     cmd    = message->ioctlv.command;

			/* Parse IOCTLV */
			ret = __MLoad_Ioctlv(cmd, vector, inlen, iolen);

			break;
		}

		default:
			/* Unknown command */
			ret = IPC_EINVAL;
		}

		/* Acknowledge message */
		os_message_queue_ack(message, ret);
	}
   
	return 0;
}
