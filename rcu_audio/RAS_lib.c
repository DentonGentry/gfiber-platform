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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "RAS_lib.h"

static int16 PV_Dec;
static int8 SI_Dec;


#define NULL	0
#define PRED_CYPHER_CONST 0x3292
#define STEP_CYPHER_CONST 0x5438

#define HDR_NOT_SCRAMBLED 1


static uint8 ras_pec_mode;
static int16 per_buff[MAX_INPUT_BUF_SIZE*4];

const uint16 codec_stepsize_Lut[89] =
{
  7,    8,    9,   10,   11,   12,   13,   14,
  16,   17,   19,   21,   23,   25,   28,   31,
  34,   37,   41,   45,   50,   55,   60,   66, 73,   80,   88,   97,  107,  118,  130,  143,
  157,  173,  190,  209,  230,  253,  279,  307, 337,  371,  408,  449,  494,  544,  598,  658,
  724,  796,  876,  963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
  3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,10442,11487,12635,13899,
  15289,16818,18500,20350,22385,24623,27086,29794, 32767
};

const int8 codec_IndexLut[16] =
{
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
};


/**************************************************************************************************
*
* @fn          codec_DecodeSingle
*
* @brief       This routine decode a 4bits ADPCM sample to a uin16 PCM audio sample.
*
* @param       uint8 a 4 bits ADPCM sample
*
*
* @return      the 16 bits PCM samples.
*/
static int16 codec_DecodeSingle(uint8 codec_4bits)
{
  int16 step = codec_stepsize_Lut[SI_Dec];
  int16 cum_diff  = step>>3;

	// DBG("step %d cum_diff %d\n", step, cum_diff);

  SI_Dec += codec_IndexLut[codec_4bits];
  if(SI_Dec<0) SI_Dec = 0; else if(SI_Dec>88) SI_Dec = 88;

  if(codec_4bits&4)
     cum_diff += step;
  if(codec_4bits&2)
     cum_diff += step>>1;
  if(codec_4bits&1)
     cum_diff += step>>2;

   if(codec_4bits&8)
   {
     if (PV_Dec < (-32767+cum_diff))
       (PV_Dec) = -32767;
     else
       PV_Dec -= cum_diff;
   }
   else
   {
     if (PV_Dec > (0x7fff-cum_diff))
       (PV_Dec) = 0x7fff;
     else
     PV_Dec += cum_diff;
   }
  return PV_Dec;
}

/**************************************************************************************************
 *
 * @fn          codec_DecodeBuff
 *
 * @brief       This routine encode a buffer with ADPCM IMA.
 *
 * @param       int16* dst  pointer to buffer where decoding result will be copy
 *              uint8* src  input buffer, size must be a multiple of 4 bytes
 *              srcSize     Number of byte that will be generated by the encoder (4* (src buffer size in byte))
 *
 *
 * @return      none
 */
static void codec_DecodeBuff(int16* dst, uint8* src, unsigned int srcSize,  int8 *si, int16 *pv)
{

  // calculate pointers to iterate output buffer
  int16* out = dst;
  int16* end = out+(srcSize>>1);
	int16 temp;

  PV_Dec = *pv;
  SI_Dec = *si;

  while(out<end)
  {
     // get byte from src
     uint8 codec = *src;
	// DBG("codec %04x\n", codec);
     // *out++ = codec_DecodeSingle((codec&0xF));  // decode value and store it
     temp = codec_DecodeSingle((codec&0xF));  // decode value and store it
		// DBG("from low %04x\n", temp);
		*out++ = temp;
     codec >>= 4;  // use high nibble of byte
     codec &= 0xF;  // use high nibble of byte
     // *out++ = codec_DecodeSingle(codec);  // decode value and store it
     temp = codec_DecodeSingle((codec));  // decode value and store it
		// DBG("from high %04x\n", temp);
		*out++ = temp;
     ++src;        // move on a byte for next sample
  }

  *pv = PV_Dec;
  *si = SI_Dec;
}


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
 *codec_ima_DecodeBuff
 * None.
 *
 * @return      .
 * status.	0				SUCCESS
 * 			-1				ERROR: INVALID PARAMETER
 */
uint8 RAS_Init( uint8 pec_mode )
{
	uint16 i;
	if (pec_mode>RAS_PEC_MODE1) return -1;
	ras_pec_mode = pec_mode;

	for (i=0; i<(MAX_INPUT_BUF_SIZE*4);i++)
		per_buff[i]=0;

	return 0;
}

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
uint16 RAS_GetVersion( void )
{
	return RAS_SOFTWARE_VERSION;
}
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
 * @param   inputLength:  	length of the buffer to decode, excluding the 3 bytes header.
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
uint8 RAS_Decode( uint8 option, uint8* input, uint16 inputLength, int16* output,uint16 *outputLenght )
{
    int8 step_index;
    int16 predicted_value;
    uint16 i;
    static uint8 *rf_DataFrame;
    *outputLenght = 0;

	// DBG("RAS_Decode option %d input %04x inputLength %d output %04x outputLength %d\n", option, input, inputLength, output, outputLenght);

    if ((output == NULL) || (inputLength > MAX_INPUT_BUF_SIZE)) return -1;

#ifdef HDR_NOT_SCRAMBLED
	predicted_value = (input [0] + ((input[1])<<8));
    step_index = input [2] & 0xFF;
#else
    predicted_value = (int16)(((int16)((input[0])<<8))+((int16)(input [2] ))) ^ PRED_CYPHER_CONST;
    step_index      = (input [1] & 0xFF) ^STEP_CYPHER_CONST;
#endif

    //extract Predicted value and step index from the header.
    inputLength-=3;  //Remove Header Size
    // check Option
    switch(option)
    {
    	case RAS_PACKET_LOST:
    	{
    		switch (ras_pec_mode)
    		{
				case RAS_PEC_MODE1:
					for (i=0; i<(inputLength*4);i++)
						output[i] = per_buff[i];
					break;
		    	default:
		    		break;
    		}

    	}
    	break;

    	case RAS_DECODE_TI_TYPE1:
    	    if (input == NULL) return -1;
    	    rf_DataFrame = input+3;
    	    codec_DecodeBuff(output, rf_DataFrame, inputLength*4,  &step_index, &predicted_value);

			//Save Frame for packet error concealment
	   		switch (ras_pec_mode)
			{
				case RAS_PEC_MODE1:
					for (i=0; i<(inputLength*4);i++)
						per_buff[i] = output[i];
					break;
				default:
					break;
			}

    		break;

    	default:
    		break;


    }
    *outputLenght = inputLength*4;
    return 0;
};


