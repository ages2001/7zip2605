/* Native archive-engine fixture, not a finished File Manager. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../C/7z.h"
#include "../../C/7zCrc.h"
#include <windows.h>
typedef struct { ISeekInStream iface; FILE *file; } Input;
static SRes readInput(ISeekInStreamPtr p,void *data,size_t *n){Input *s=(Input *)p;*n=fread(data,1,*n,s->file);return ferror(s->file)?SZ_ERROR_READ:SZ_OK;}
static SRes seekInput(ISeekInStreamPtr p,Int64 *pos,ESzSeek origin){Input *s=(Input *)p;long offset=(long)*pos;if((Int64)offset!=*pos)return SZ_ERROR_UNSUPPORTED;if(fseek(s->file,offset,(int)origin))return SZ_ERROR_READ;offset=ftell(s->file);if(offset<0)return SZ_ERROR_READ;*pos=offset;return SZ_OK;}
static void *alloc(ISzAllocPtr p,size_t n){(void)p;return malloc(n);}
static void release(ISzAllocPtr p,void *v){(void)p;free(v);}
int PASCAL WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show){
 static Byte lookBuffer[4096];Input input;CLookToRead2 stream;CSzArEx archive;ISzAlloc allocator={alloc,release};SRes result;Byte *out=0;size_t outSize=0,offset=0,processed=0;UInt32 block=(UInt32)-1;FILE *log;unsigned i;int valid=0;static const char expected[]="7-Zip outbound file drop: exact bytes\r\n";
 (void)instance;(void)previous;(void)command;(void)show;
 input.iface.Read=readInput;input.iface.Seek=seekInput;input.file=fopen("C:\\WIN16.7Z","rb");if(!input.file)return 2;
 LookToRead2_CreateVTable(&stream,False);stream.realStream=&input.iface;stream.buf=lookBuffer;stream.bufSize=sizeof(lookBuffer);LookToRead2_INIT(&stream);SzArEx_Init(&archive);CrcGenerateTable();
 result=SzArEx_Open(&archive,&stream.vt,&allocator,&allocator);
 if(result==SZ_OK&&archive.NumFiles==1){result=SzArEx_Extract(&archive,&stream.vt,0,&block,&out,&outSize,&offset,&processed,&allocator,&allocator);valid=result==SZ_OK&&processed==4096;for(i=0;valid&&i<processed;i++)if(out[offset+i]!=(Byte)expected[i%(sizeof(expected)-1)])valid=0;}
 log=fopen("C:\\ARC16.TXT","w");if(log){fprintf(log,"Native Win16 shared 7z archive engine\nGetWinFlags=%lu result=%d files=%lu bytes=%u\n%s\n",GetWinFlags(),result,archive.NumFiles,processed,valid?"PASS exact archive bytes":"FAIL");fclose(log);}
 free(out);SzArEx_Free(&archive,&allocator);fclose(input.file);MessageBox(0,valid?"PASS: real .7z archive decoded with shared source.":"FAIL: archive engine fixture.","Win16 archive engine test",MB_OK);return valid?0:1;
}
