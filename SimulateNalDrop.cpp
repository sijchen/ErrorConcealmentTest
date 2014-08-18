
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <math.h>
#include <assert.h>

using namespace std;

enum{
  MODE_GET_NAL_LIST=0,
  MODE_DROP_NAL=1,
};

enum{
  NAL_TYPE_IDR=5,
  NAL_TYPE_P=1,
  NAL_TYPE_SPS=7,
  NAL_TYPE_PPS=8,
};



typedef struct TagNal
{
  int iNalIdx;
  unsigned char* pDataStart;
  int iNalLength;
  int iNalType;
}SNal;
typedef struct TagAUInfo
{
  int iAUIdx;
  int iAUPacketNum;
  unsigned char* pDataStart;
  int iAULengthInBytes;
  int iAUStartPackeIdx;
}SAUInfo;
typedef vector<SNal>VH264Nal;
typedef vector<SAUInfo>VAUUnit;

const unsigned char g_kuiLeadingZeroTable[256] = {
  8,  7,  6,  6,  5,  5,  5,  5,  4,  4,  4,  4,  4,  4,  4,  4,
  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
typedef struct TagBitStringAux {
  unsigned char*		pStartBuf;	// buffer to start position
  unsigned char*		pEndBuf;	// buffer + length
  int     iBits;       // count bits of overall bitstreaming input

  int     iIndex;      //only for cavlc usage
  unsigned char*		pCurBuf;	// current reading position
  unsigned int    uiCurBits;
  int		iLeftBits;	// count number of available bits left ([1, 8]),
// need pointer to next byte start position in case 0 bit left then 8 instead
} SBitStringAux, *PBitStringAux;
#define GET_WORD(iCurBits, pBufPtr, iLeftBits, iAllowedBytes, iReadBytes) { \
  if (iReadBytes > iAllowedBytes+1) { \
    return 1; \
  } \
	iCurBits |= ((unsigned int)((pBufPtr[0] << 8) | pBufPtr[1])) << (iLeftBits); \
	iLeftBits -= 16; \
	pBufPtr +=2; \
}
#define NEED_BITS(iCurBits, pBufPtr, iLeftBits, iAllowedBytes, iReadBytes) { \
	if( iLeftBits > 0 ) { \
	GET_WORD(iCurBits, pBufPtr, iLeftBits, iAllowedBytes, iReadBytes); \
	} \
}
#define UBITS(iCurBits, iNumBits) (iCurBits>>(32-(iNumBits)))
#define DUMP_BITS(iCurBits, pBufPtr, iLeftBits, iNumBits, iAllowedBytes, iReadBytes) { \
	iCurBits <<= (iNumBits); \
	iLeftBits += (iNumBits); \
	NEED_BITS(iCurBits, pBufPtr, iLeftBits, iAllowedBytes, iReadBytes); \
}
inline unsigned int GetValue4Bytes (unsigned char* pDstNal) {
  unsigned int uiValue = 0;
  uiValue = (pDstNal[0] << 24) | (pDstNal[1] << 16) | (pDstNal[2] << 8) | (pDstNal[3]);
  return uiValue;
}
void InitReadBits (PBitStringAux pBitString) {
  pBitString->uiCurBits  = GetValue4Bytes (pBitString->pCurBuf);
  pBitString->pCurBuf  += 4;
  pBitString->iLeftBits = -16;
}
int InitBits (PBitStringAux pBitString, const  unsigned char* kpBuf, const int kiSize) {
  const int kiSizeBuf = (kiSize + 7) >> 3;
  unsigned char* pTmp = (unsigned char*)kpBuf;

  if (NULL == pTmp)
    return -1;

  pBitString->pStartBuf   = pTmp;				// buffer to start position
  pBitString->pEndBuf	    = pTmp + kiSizeBuf;	// buffer + length
  pBitString->iBits	    = kiSize;				// count bits of overall bitstreaming inputindex;

  pBitString->pCurBuf   = pBitString->pStartBuf;
  InitReadBits (pBitString);

  return kiSizeBuf;
}
static inline int GetLeadingZeroBits (unsigned int iCurBits) { //<=32 bits
  unsigned int  uiValue;

  uiValue = UBITS (iCurBits, 8); //ShowBits( bs, 8 );
  if (uiValue) {
    return g_kuiLeadingZeroTable[uiValue];
  }

  uiValue = UBITS (iCurBits, 16); //ShowBits( bs, 16 );
  if (uiValue) {
    return (g_kuiLeadingZeroTable[uiValue] + 8);
  }

  uiValue = UBITS (iCurBits, 24); //ShowBits( bs, 24 );
  if (uiValue) {
    return (g_kuiLeadingZeroTable[uiValue] + 16);
  }

  uiValue = iCurBits; //ShowBits( bs, 32 );
  if (uiValue) {
    return (g_kuiLeadingZeroTable[uiValue] + 24);
  }
//ASSERT(false);  // should not go here
  return -1;
}
static inline unsigned int BsGetUe (PBitStringAux pBs) {
  unsigned int iValue = 0;
  int  iLeadingZeroBits = GetLeadingZeroBits (pBs->uiCurBits);
  int iAllowedBytes, iReadBytes;
  iAllowedBytes = pBs->pEndBuf - pBs->pStartBuf; //actual stream bytes

  if (iLeadingZeroBits == -1) { //bistream error
    return -1;
  } else if (iLeadingZeroBits >
             16) { //rarely into this condition (even may be bitstream error), prevent from 16-bit reading overflow
    //using two-step reading instead of one time reading of >16 bits.
    iReadBytes = pBs->pCurBuf - pBs->pStartBuf;
    DUMP_BITS (pBs->uiCurBits, pBs->pCurBuf, pBs->iLeftBits, 16, iAllowedBytes, iReadBytes);
    iReadBytes = pBs->pCurBuf - pBs->pStartBuf;
    DUMP_BITS (pBs->uiCurBits, pBs->pCurBuf, pBs->iLeftBits, iLeadingZeroBits + 1 - 16, iAllowedBytes, iReadBytes);
  } else {
    iReadBytes = pBs->pCurBuf - pBs->pStartBuf;
    DUMP_BITS (pBs->uiCurBits, pBs->pCurBuf, pBs->iLeftBits, iLeadingZeroBits + 1, iAllowedBytes, iReadBytes);
  }
  if (iLeadingZeroBits) {
    iValue = UBITS (pBs->uiCurBits, iLeadingZeroBits);
    iReadBytes = pBs->pCurBuf - pBs->pStartBuf;
    DUMP_BITS (pBs->uiCurBits, pBs->pCurBuf, pBs->iLeftBits, iLeadingZeroBits, iAllowedBytes, iReadBytes);
  }
  return ((1 << iLeadingZeroBits) - 1 + iValue);
}

#define IsKeyFrame(type) (NAL_TYPE_SPS==type || NAL_TYPE_PPS==type || NAL_TYPE_IDR==type)
#define exist_annexb_nal_header(bs) \
	((0 == bs[0]) && (0 == bs[1]) && (0 == bs[2]) && (1 == bs[3]))
int detect_nal_length( unsigned char *buf_start, const int left_len )
{
	int iOffset						= 4;
	unsigned char *cur_bs			= buf_start + 4;	

	//get to the next NAL header
	while( iOffset < left_len )
	{
		int count_leading_zero = 0;
		while ( 0 == *cur_bs )
		{
			++ count_leading_zero;
			++ iOffset;
			++ cur_bs;
			if ( (count_leading_zero >= 3) && (iOffset < left_len) && (1 == *cur_bs) )
			{
				iOffset -= 3;
				return iOffset;
			}		
		}		
		++ cur_bs;
		++ iOffset;
	}
	return iOffset;
}



////////////////////////
void ReadBs(unsigned char *src_bs_buffer, int iSrcBsLength,
            VH264Nal& vH264Nal, VH264Nal& vIdrNal, VAUUnit& vAUUnit) {
  unsigned char *pCurrentSrc = src_bs_buffer;
  unsigned char *pLastNalStartSrc = pCurrentSrc;
  SNal sNal = {0};
  int iCurNalIdx = 0, iCurAUIdx = 0;
  int kiNalLen = 0, iAUPackets=0, iAuLen = 0, iLastFirstMb =0, iAUStartPackeIdx=0;
  SAUInfo sAU = {0};
  while (src_bs_buffer + iSrcBsLength- pCurrentSrc > 5) {
    if( exist_annexb_nal_header(pCurrentSrc) && pCurrentSrc != src_bs_buffer) {
      kiNalLen = pCurrentSrc - pLastNalStartSrc;
      // put last NAL to NAL list
      sNal.iNalIdx = iCurNalIdx;
      sNal.pDataStart = pLastNalStartSrc;
      sNal.iNalLength =  kiNalLen;
      sNal.iNalType = (pLastNalStartSrc[4] & 0x1F);
      vH264Nal.push_back(sNal);
      iAUPackets++;
      iAuLen+=kiNalLen;

      if IsKeyFrame(sNal.iNalType) {
        vIdrNal.push_back(sNal);
      }

      //check AU
      bool bNewAU = false;
      int iCurNalType = (pCurrentSrc[4] & 0x1F);
      if (NAL_TYPE_P == iCurNalType) {
        SBitStringAux sBitStringAux;
        InitBits (&sBitStringAux, pLastNalStartSrc+5, kiNalLen);
        int iFirstMb = BsGetUe(&sBitStringAux);
        if (iFirstMb == 0 || iFirstMb < iLastFirstMb) {bNewAU = true;}
        iLastFirstMb = iFirstMb;
      }
      if (NAL_TYPE_SPS == iCurNalType){bNewAU = true;}

      if (bNewAU) {
        sAU.iAUIdx = iCurAUIdx;
        sAU.iAUPacketNum = iAUPackets;
        sAU.iAULengthInBytes = iAuLen;
        sAU.iAUStartPackeIdx = iAUStartPackeIdx;
        sAU.pDataStart = vH264Nal[iAUStartPackeIdx].pDataStart;
        vAUUnit.push_back(sAU);
        iCurAUIdx ++;
        iAUStartPackeIdx += iAUPackets;
        iAUPackets = 0;
        iAuLen = 0;
      }

      iCurNalIdx ++;
      pLastNalStartSrc = pCurrentSrc;
    }
    ++ pCurrentSrc;    
  }

    //last NAL
  kiNalLen = pCurrentSrc - pLastNalStartSrc;
  if (kiNalLen>0) {
    sNal.iNalIdx = iCurNalIdx;
    sNal.pDataStart = pLastNalStartSrc;
    sNal.iNalLength =  kiNalLen;
    sNal.iNalType = (pLastNalStartSrc[4] & 0x1F);
    vH264Nal.push_back(sNal);

    sAU.iAUIdx = iCurAUIdx;
    sAU.iAUPacketNum = iAUPackets;
    sAU.iAULengthInBytes = iAuLen;
    sAU.iAUStartPackeIdx = iAUStartPackeIdx;
    sAU.pDataStart = vH264Nal[iAUStartPackeIdx].pDataStart;
    vAUUnit.push_back(sAU);
  }
}

int main(int argc, char* argv[])
{
  printf("usage: mode Src ## MODE_GET_NAL_LIST\n");
  printf("usage: mode Src KeyFramePeriod ImpairedPositionAfterKeyFrame NumOfLossNal ## MODE_DROP_NAL \n");

  if (argc<2)
  {
    fprintf(stdout, "wrong argv, see help\n")
  }
  const int iMode= atoi(argv[1]);

  unsigned char *src_bs_buffer = NULL;
  unsigned char *target_bs_buffer= NULL;
  FILE *fpSrc = fopen(argv[2], "rb");
  
  if (MODE_GET_NAL_LIST==iMode)
  {     
    char filename[100];
    strcpy(filename, argv[2]);
    strcat(filename, ".info");
    FILE *fpDstLen = fopen(filename, "w");

    if (fpSrc && fpDstLen && !fseek (fpSrc, 0, SEEK_END)) {
      int src_bs_len = ftell (fpSrc);
      fseek (fpSrc, 0, SEEK_SET);

      if (src_bs_len > 0) {
        src_bs_buffer = new unsigned char[src_bs_len];
        if (NULL == src_bs_buffer ) {
          fprintf(stderr, "out of memory due src bs buffer memory request(size=%d)\n", src_bs_len);
        }else{
          if ( fread(src_bs_buffer, sizeof(unsigned char), src_bs_len, fpSrc) < src_bs_len*sizeof(unsigned char) ) {
            fprintf(stderr, "fread failed!\n");
            delete []src_bs_buffer;}

          if (NULL != src_bs_buffer){
            VH264Nal vDataPacket;
            VH264Nal vIdrNalList;
            VAUUnit vAUUnit;
            ReadBs(src_bs_buffer, src_bs_len, vDataPacket, vIdrNalList, vAUUnit);

            for (int k=0;k<vDataPacket.size();k++) {
              fprintf(fpDstLen, "NAl#\t%d\t, iNalType=\t%d\t, iLengthInBytes=\t%d\n", 
                vDataPacket[k].iNalIdx,
                vDataPacket[k].iNalType,
                vDataPacket[k].iNalLength);
            }

             for (int k=0;k<vAUUnit.size();k++) {
              fprintf(fpDstLen, "vAUUnit#\t%d\t, iPackets=\t%d\t, iAUStartPackeIdx=%d, iLengthInBytes=\t%d\n", 
                vAUUnit[k].iAUIdx,
                vAUUnit[k].iAUPacketNum,vAUUnit[k].iAUStartPackeIdx,
                vAUUnit[k].iAULengthInBytes);
            }
          }
        }
      }
    }
    delete []src_bs_buffer;
    if (fpDstLen!=NULL)    fclose(fpDstLen);
  }
 
  if (MODE_DROP_NAL==iMode) {
    //KeyFramePeriod ImpairedPositionAfterKeyFrame NumOfLossNal
    const int iKeyFramePeriod= atoi(argv[3]);
    const int iImpairedPosition= atoi(argv[4]);
    const int iNumOfMaxLossNal= atoi(argv[5]);
    if (iImpairedPosition == 0 || iNumOfMaxLossNal == 0) {
      printf("unexpected input iImpairedPosition=%d, iNumOfMaxLossNal=%d\n", iImpairedPosition, iNumOfMaxLossNal);
      goto exit_tag;
    }

    char filename[100];
    strcpy(filename, argv[2]);
    strcat(filename, "_impaired.264");
    FILE *fpDst = fopen(filename, "wb");
    if (fpSrc && !fseek (fpSrc, 0, SEEK_END)) {
      int src_bs_len = ftell (fpSrc);
      fseek (fpSrc, 0, SEEK_SET);

      if (src_bs_len > 0) {
        src_bs_buffer = new unsigned char[src_bs_len];
        if (NULL == src_bs_buffer ) {
          fprintf(stderr, "out of memory due src bs buffer memory request(size=%d)\n", src_bs_len);
        }else{
          if ( fread(src_bs_buffer, sizeof(unsigned char), src_bs_len, fpSrc) < src_bs_len*sizeof(unsigned char) ) {
            fprintf(stderr, "fread failed!\n");
            delete []src_bs_buffer;}

          if (NULL != src_bs_buffer){
            fprintf(stdout, "BeginReadBs: File Len=%d\n", src_bs_len);
            VH264Nal vDataPacket;
            VH264Nal vIdrNalList;
            VAUUnit vAUUnit;
            ReadBs(src_bs_buffer, src_bs_len, vDataPacket, vIdrNalList, vAUUnit);
            fprintf(stdout, "AfterReadBs: %d frames found\n", vAUUnit.size());

            int  iNonKeyCount = 0, iStartIdx=0, iTtlPackets=0;
            int  iCopyEnd = 0, iNalType = 0;
            int iNumOfLossNal = iNumOfMaxLossNal;
            for (int k=0;k<vAUUnit.size();k++) {

              if (k%iKeyFramePeriod){iNonKeyCount++;}
              else { //is key frame
                iNonKeyCount = 0;
                iNumOfLossNal = iNumOfMaxLossNal;
              }
               
              if (iNonKeyCount >= iImpairedPosition && iNumOfLossNal>0)
              {
                if (iNumOfLossNal>=vAUUnit[k].iAUPacketNum){
                  iNumOfLossNal -= vAUUnit[k].iAUPacketNum;
                  fprintf(stdout, "whole frame dropped: %d\n", k);
                }
                else {
                  iStartIdx = vAUUnit[k].iAUStartPackeIdx;
                  iTtlPackets = vAUUnit[k].iAUPacketNum;
                  int  iLossStartIdx = (iTtlPackets-iNumOfLossNal)/2;
                  for (int n=0; n<iTtlPackets; n++){
                    if (n<iLossStartIdx || n>=iLossStartIdx+iNumOfLossNal)
                      fwrite( vDataPacket[n+iStartIdx].pDataStart, 1, vDataPacket[n+iStartIdx].iNalLength, fpDst );    
                  }
                  iNumOfLossNal = 0;
                  fprintf(stdout, "impaired frame: %d\n", k);
                }
                
              }
              else {
                fwrite( vAUUnit[k].pDataStart, 1, vAUUnit[k].iAULengthInBytes, fpDst );      
              }
              
            }

          }
        }
      }
    }
    delete []src_bs_buffer;
    if (fpDst!=NULL)    fclose(fpDst);
  }
   
exit_tag:
  if (fpSrc!=NULL)    fclose(fpSrc);
  printf("Finished mode=%d!\n", iMode);
  return 0;
}

