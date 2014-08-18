#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

typedef unsigned char uint8_t;
typedef int int32_t;

enum ToolMode
{
  COPY_FROM_ERROR_FRAME=0,
  COPY_ONE_ERROR_FRAME=1,
};

void WriteFrame(const uint8_t* pSrc,  const int32_t iSrcWidth, const int32_t iTargetWidth, const int32_t iTargetHeight, 
                FILE* fp)
{
    for (int j=0;j<iTargetHeight;j++)
    {
        fwrite( pSrc+j*iSrcWidth, 1, iTargetWidth, fp );
    }
}
void ModiFrame(const uint8_t* pSrc,  const int32_t iSrcWidth, const int32_t iTargetWidth, const int32_t iTargetHeight, 
                FILE* fp)
{
#define AVER_COLOR
    int32_t i=0, j=0;
    int32_t value=0;

    for (j=0;j<iTargetHeight;j++)
    {
        if (j%2==0)
        {
            for (i=0;i<iTargetWidth;i++)
            {
                if (i%2==0)
                {
#ifdef AVER_COLOR
                    value = *(pSrc+j*iSrcWidth+i)
                        + *(pSrc+j*iSrcWidth+i+1) 
                        + *(pSrc+(j+1)*iSrcWidth+i)
                        + *(pSrc+(j+1)*iSrcWidth+i+1);
                        value = value>>2;
                    fwrite( &value, 1, 1, fp );
#else
                    fwrite( pSrc+j*iSrcWidth+i, 1, 1, fp );
#endif
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
  printf("usage: mode(0) iWidth iHeight Src.yuv Dst.yuv KeyFramePeriod ImpairedPositionAfterKeyFrame# \n");
  printf("usage: mode(1) iWidth iHeight Src.yuv Dst.yuv TheFrameToCopy(from0) CopyTimes\n");

  const int32_t iMode= atoi(argv[1]);
  const int32_t iWidth = atoi(argv[2]);    
  const int32_t iHeight = atoi(argv[3]);

  const int32_t iCWidth =iWidth>>1;    
  const int32_t iCHeight = iHeight>>1;
  const int32_t iLumaSize = iWidth*iHeight;
  const int32_t iChromaSize = iCWidth*iCHeight;

  uint8_t *pSrcY = (uint8_t *)malloc(iLumaSize); 
  uint8_t *pSrcU = (uint8_t *)malloc(iLumaSize); 
  uint8_t *pSrcV = (uint8_t *)malloc(iLumaSize); 

  uint8_t *pCopyY = (uint8_t *)malloc(iLumaSize); 
  uint8_t *pCopyU = (uint8_t *)malloc(iLumaSize); 
  uint8_t *pCopyV = (uint8_t *)malloc(iLumaSize); 

  int32_t i, j, k;

  if (iMode == COPY_FROM_ERROR_FRAME)
  {
    FILE *fpSrc = fopen(argv[4], "rb");
    FILE *fpDst = fopen(argv[5], "wb");    
    const int32_t iKeyFramePeriod = atoi(argv[6]);
    const int32_t iErrorFrame = atoi(argv[7]);
    int iFileLen = 0;
    
    if (fpSrc && !fseek (fpSrc, 0, SEEK_END)) {
        iFileLen = ftell (fpSrc);
        fseek (fpSrc, 0, SEEK_SET);
    }

    j=0;
    int iFrameSize = iLumaSize+iChromaSize+iChromaSize;
     int iLeftLength = iFileLen;
    //rewrite each frame
    while( EOF != ftell(fpSrc) && iLeftLength )
    {	
      fread( pSrcY, 1, iLumaSize, fpSrc );
      fread( pSrcU, 1, iChromaSize, fpSrc );
      fread( pSrcV, 1, iChromaSize, fpSrc );

      if (j%iKeyFramePeriod == iErrorFrame-1)
      {//at the error frame. update copy buffer
        memcpy(pCopyY, pSrcY, iLumaSize*sizeof(unsigned char));
        memcpy(pCopyU, pSrcU, iChromaSize*sizeof(unsigned char));
        memcpy(pCopyV, pSrcV, iChromaSize*sizeof(unsigned char));
      }
      
      if (j%iKeyFramePeriod >= iErrorFrame){
        WriteFrame(pCopyY, iWidth, iWidth, iHeight, fpDst);
        WriteFrame(pCopyU, iWidth>>1, iWidth>>1, iHeight>>1, fpDst);
        WriteFrame(pCopyV, iWidth>>1, iWidth>>1, iHeight>>1, fpDst); 
      }
      else {
        WriteFrame(pSrcY, iWidth, iWidth, iHeight, fpDst);
        WriteFrame(pSrcU, iWidth>>1, iWidth>>1, iHeight>>1, fpDst);
        WriteFrame(pSrcV, iWidth>>1, iWidth>>1, iHeight>>1, fpDst);
      }

      iLeftLength -= iFrameSize;
      j++;
    }

    fclose(fpDst);
    fclose(fpSrc);
  }
  if (iMode == COPY_ONE_ERROR_FRAME)
  {
    FILE *fpSrc = fopen(argv[4], "rb");
    FILE *fpDst = fopen(argv[5], "wb");    
    const int32_t iTheFrameToCopy = atoi(argv[6]);
    const int32_t iCopyTimes = atoi(argv[7]);
    int iFileLen = 0;
    
    if (fpSrc && !fseek (fpSrc, 0, SEEK_END)) {
        iFileLen = ftell (fpSrc);
        fseek (fpSrc, 0, SEEK_SET);
    }

    j=0;
    int iFrameSize = iLumaSize+iChromaSize+iChromaSize;
     int iLeftLength = iFileLen;
    //rewrite each frame
    while( EOF != ftell(fpSrc) && iLeftLength )
    {	
      fread( pSrcY, 1, iLumaSize, fpSrc );
      fread( pSrcU, 1, iChromaSize, fpSrc );
      fread( pSrcV, 1, iChromaSize, fpSrc );

      if (j == iTheFrameToCopy)
      {//before the error frame. update copy buffer
        memcpy(pCopyY, pSrcY, iLumaSize*sizeof(unsigned char));
        memcpy(pCopyU, pSrcU, iChromaSize*sizeof(unsigned char));
        memcpy(pCopyV, pSrcV, iChromaSize*sizeof(unsigned char));
      }
      
      if (j == iTheFrameToCopy+1){
        for (int k=0;k<iCopyTimes;k++)
        {
        WriteFrame(pCopyY, iWidth, iWidth, iHeight, fpDst);
        WriteFrame(pCopyU, iWidth>>1, iWidth>>1, iHeight>>1, fpDst);
        WriteFrame(pCopyV, iWidth>>1, iWidth>>1, iHeight>>1, fpDst); 
        }
      }

      //write the currect frame
      WriteFrame(pSrcY, iWidth, iWidth, iHeight, fpDst);
      WriteFrame(pSrcU, iWidth>>1, iWidth>>1, iHeight>>1, fpDst);
      WriteFrame(pSrcV, iWidth>>1, iWidth>>1, iHeight>>1, fpDst);

      iLeftLength -= iFrameSize;
      j++;
    }

    fclose(fpDst);
    fclose(fpSrc);
  }
  free(pSrcY);
  free(pSrcU);
  free(pSrcV);
  free(pCopyY);
  free(pCopyU);
  free(pCopyV);
  printf("Finish dealing!\n");
  return 0;
}

