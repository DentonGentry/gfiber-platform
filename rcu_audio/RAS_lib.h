/*
* Copyright (c) [2015] Texas Instruments Incorporated
*
* All rights reserved not granted herein.
* Limited License.
*
* Texas Instruments Incorporated grants a world-wide, royalty-free,
* non-exclusive license under copyrights and patents it now or hereafter
* owns or controls to make, have made, use, import, offer to sell and sell ("Utilize")
* this software subject to the terms herein.  With respect to the foregoing patent
*license, such license is granted  solely to the extent that any such patent is necessary
* to Utilize the software alone.  The patent license shall not apply to any combinations which
* include this software, other than combinations with devices manufactured by or for TI ("TI Devices").
* No hardware patent is licensed hereunder.
*
* Redistributions must preserve existing copyright notices and reproduce this license (including the
* above copyright notice and the disclaimer and (if applicable) source code license limitations below)
* in the documentation and/or other materials provided with the distribution
*
* Redistribution and use in binary form, without modification, are permitted provided that the
* following conditions are met:
*
*             * No reverse engineering, decompilation, or disassembly of this software is permitted
*             	with respect to any software provided in binary form.
*             * any redistribution and use are licensed by TI for use only with TI Devices.
*             * Nothing shall obligate TI to provide you with source code for the software licensed
*             	and provided to you in object code.
*
* If software source code is provided to you, modification and redistribution of the source code are
* permitted provided that the following conditions are met:
*
*   * any redistribution and use of the source code, including any resulting derivative works, are
*     licensed by TI for use only with TI Devices.
*   * any redistribution and use of any object code compiled from the source code and any resulting
*     derivative works, are licensed by TI for use only with TI Devices.
*
* Neither the name of Texas Instruments Incorporated nor the names of its suppliers may be used to
* endorse or promote products derived from this software without specific prior written permission.
*
* DISCLAIMER.
*
* THIS SOFTWARE IS PROVIDED BY TI AND TI'S LICENSORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL TI AND TI'S LICENSORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef RSA_LIB_H
#define RSA_LIB_H

#ifdef __cplusplus
extern "C" {
#endif


#if !defined PACK_1
#define PACK_1
#endif


#if defined(_MSC_VER) || defined(unix) || (defined(__ICC430__) && (__ICC430__==1))
#pragma pack(1)
#endif


/////////////////////////////////////////////////////////////////////////////
// Defines

#define MAX_INPUT_BUF_SIZE 		128

#define RAS_PACKET_LOST 		0
#define RAS_DECODE_TI_TYPE1		1

#define RAS_NO_PEC		   		0
#define RAS_PEC_MODE1   		1

//RAS Software Version: v1.3
#define RAS_SOFTWARE_VERSION	0x0103
/////////////////////////////////////////////////////////////////////////////
// Typedefs
#ifndef int8
typedef signed   char   int8;
#endif

#ifndef uint8
typedef unsigned char   uint8;
#endif

#ifndef int16
typedef signed   short  int16;
#endif

#ifndef uint16
typedef unsigned short  uint16;
#endif

#ifndef int32
typedef signed   int  int32;
#endif

#ifndef uint32
typedef unsigned int  uint32;
#endif

/////////////////////////////////////////////////////////////////////////////
// Global variable


/////////////////////////////////////////////////////////////////////////////
// Function declarations
/**************************************************************************************************
 *
 * @fn      RAS_GetVersion
 *
 * @brief   RemoTI Audio Subsystem, retrieve software version
 *
 * input parameters
 *
 * none
 *
 * output parameters
 *
 * None.
 *
 * @return      .
 * Software Version.	MSB	 Major revision number
 *						LSB: Minor revision number
 */
uint16 RAS_GetVersion( void );

/**************************************************************************************************
 *
 * @fn      RAS_Init
 *
 * @brief   RemoTI Audio Subsystem, initialization function
 *
 * input parameters
 *
 * @param   pec_mode:    	Packet Error concealment algorithm to apply:
 * 							RAS_NO_PEC(0): 		None (default)
 * 							RAS_PEC_MODE1(1): 	Replace lost packets by last valid.
 *
 * output parameters
 *
 * None.
 *
 * @return      .
 * status.	0				SUCCESS
 * 			-1				ERROR: INVALID PARAMETER
 */
uint8 RAS_Init( uint8 pec_mode );


/**************************************************************************************************
 *
 * @fn      RAS_Decode
 *
 * @brief   RemoTI Audio Subsystem, decoding function. decode encoded audioframe to PCM samples.
 *
 * input parameters
 *
 * @param   option:    		decoding option. can be pure decoding, or packet lot concealment algorithm:
 * 							RAS_PACKET_LOST(0)
 * 							RAS_DECODE(1)
 * @param   input: 			address of the buffer to decode, this buffer must include the 3 bytes header..
 *
 * @param   inputLenght:  	length of the buffer to decode, excluding the 3 bytes header.
 * 							cannot be greater than 128 (MAX_INPUT_BUF_SIZE);
 *
 * output parameters
 *
 * @param   output:     	buffer where the decoded PCM will be written. This buffer must be allocated by the caller.
 * 							it must have a length of 4 times the inputLength variable
 *
 * @param   outputLenght:  	length of the decoded buffer.
 * 							max possible value 512 (4*MAX_INPUT_BUF_SIZE);
 *
 *
 * @return      .
 * status.	0				SUCCESS
 * 			-1				ERROR: INVALID PARAMETER
 *
 */
uint8 RAS_Decode( uint8 option, uint8* input, uint16 inputLenght, int16* output,uint16 *outputLenght );

#ifdef __cplusplus
}
#endif

#endif // RSA_LIB_H
