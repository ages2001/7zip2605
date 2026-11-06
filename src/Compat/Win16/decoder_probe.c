/* Native 8086 Win16 NE probe using the shared upstream LZMA decoder.
   This is a regression fixture, not the 7-Zip application. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../C/LzmaDec.h"
#include "decoder_vector.h"
#include <windows.h>
static void *allocate(ISzAllocPtr p,size_t n){(void)p;return malloc(n);}
static void release(ISzAllocPtr p,void *v){(void)p;free(v);}
int PASCAL WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show){
 static unsigned char decoded[EXPECTED_SIZE];ISzAlloc alloc={allocate,release};
 SizeT out=sizeof(decoded),in=sizeof(packed)-13;ELzmaStatus status;SRes result;
 unsigned i;int valid,bounds;CLzmaDec probe;Byte props[5];SRes limitResult;FILE *log;DWORD flags=GetWinFlags();
 (void)instance;(void)previous;(void)command;(void)show;
 result=LzmaDecode(decoded,&out,packed+13,&in,packed,5,LZMA_FINISH_END,&status,&alloc);
 valid=result==SZ_OK&&out==EXPECTED_SIZE;
 for(i=0;i<out&&valid;i++)if(decoded[i]!=(unsigned char)expectedLine[i%(sizeof(expectedLine)-1)])valid=0;
 LzmaDec_Construct(&probe);memcpy(props,packed,5);props[1]=0;props[2]=0;props[3]=16;props[4]=0;
 limitResult=LzmaDec_Allocate(&probe,props,5,&alloc);bounds=limitResult==SZ_ERROR_MEM;LzmaDec_Free(&probe,&alloc);
 LzmaDec_Construct(&probe);props[0]=44;limitResult=LzmaDec_AllocateProbs(&probe,props,5,&alloc);bounds=bounds&&limitResult==SZ_ERROR_MEM;LzmaDec_Free(&probe,&alloc);valid=valid&&bounds;
 log=fopen("C:\\WIN16.TXT","w");if(log){fprintf(log,"Native Win16 upstream LZMA decoder probe\nGetWinFlags: %lu\nMode: %s\nsizeof(int)=%u sizeof(UInt32)=%u sizeof(void*)=%u\nResult=%d status=%d input=%u output=%u\n%s\n",flags,(flags&WF_ENHANCED)?"enhanced":(flags&WF_PMODE)?"standard":"real",sizeof(int),sizeof(UInt32),sizeof(void*),result,status,in,out,valid?"PASS exact decoded bytes and allocation overflow rejection":"FAIL");fclose(log);}
 MessageBox(0,valid?"PASS: upstream LZMA decoded exact bytes.\nThis is a Win16 test fixture, not a complete 7-Zip port.":"FAIL: Win16 decoder probe","7-Zip Win16 feasibility test",MB_OK);
 return valid?0:1;
}
