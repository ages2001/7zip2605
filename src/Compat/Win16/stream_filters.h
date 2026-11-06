/* Streaming adapters for the shared branch and delta converters.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#include "../../C/Bra.h"
#include "../../C/Delta.h"
#include "../../C/CpuArch.h"

#define Z7_FILTER_BUFFER 4112
typedef struct
{
  ISeqOutStream iface;
  ISeqOutStreamPtr next;
  Byte *buffer;
  size_t pending;
  UInt32 method, pc, x86State;
  unsigned distance;
  Byte deltaState[DELTA_STATE_SIZE];
} FilterStream;

static SRes filterProperties(FilterStream *filter, const CSzCoderInfo *coder,
    const Byte *properties)
{
  filter->method=coder->MethodID;
  filter->pc=0;filter->x86State=Z7_BRANCH_CONV_ST_X86_STATE_INIT_VAL;
  filter->pending=0;filter->buffer=NULL;
  switch(filter->method)
  {
    case 3:
      if(coder->PropsSize!=1)return SZ_ERROR_DATA;
      filter->distance=(unsigned)properties[coder->PropsOffset]+1;
      Delta_Init(filter->deltaState);
      return SZ_OK;
    case 0xa: case 0xb:
      if(coder->PropsSize==4)
      {
        filter->pc=GetUi32(properties+coder->PropsOffset);
        if(filter->pc&(filter->method==0xa?3:1))return SZ_ERROR_DATA;
      }
      else if(coder->PropsSize!=0)return SZ_ERROR_DATA;
      return SZ_OK;
    case 0x3030103UL: case 0x3030205UL: case 0x3030401UL:
    case 0x3030501UL: case 0x3030701UL: case 0x3030805UL:
      return coder->PropsSize==0?SZ_OK:SZ_ERROR_DATA;
    default:return SZ_ERROR_UNSUPPORTED;
  }
}

static size_t filterConvert(FilterStream *filter)
{
  Byte *begin=filter->buffer,*end=begin;
  switch(filter->method)
  {
    case 3:
      Delta_Decode(filter->deltaState,filter->distance,begin,filter->pending);
      return filter->pending;
    case 0x3030103UL:
      end=Z7_BRANCH_CONV_ST_DEC(X86)(begin,filter->pending,filter->pc,&filter->x86State);break;
    case 0x3030205UL:end=Z7_BRANCH_CONV_DEC(PPC)(begin,filter->pending,filter->pc);break;
    case 0x3030401UL:end=Z7_BRANCH_CONV_DEC(IA64)(begin,filter->pending,filter->pc);break;
    case 0x3030501UL:end=Z7_BRANCH_CONV_DEC(ARM)(begin,filter->pending,filter->pc);break;
    case 0x3030701UL:end=Z7_BRANCH_CONV_DEC(ARMT)(begin,filter->pending,filter->pc);break;
    case 0x3030805UL:end=Z7_BRANCH_CONV_DEC(SPARC)(begin,filter->pending,filter->pc);break;
    case 0xa:end=Z7_BRANCH_CONV_DEC(ARM64)(begin,filter->pending,filter->pc);break;
    case 0xb:end=Z7_BRANCH_CONV_DEC(RISCV)(begin,filter->pending,filter->pc);break;
  }
  return (size_t)(end-begin);
}

static size_t filterWrite(ISeqOutStreamPtr stream,const void *data,size_t size)
{
  FilterStream *filter=(FilterStream *)stream;
  const Byte *input=(const Byte *)data;
  size_t left=size;
  while(left)
  {
    size_t n=Z7_FILTER_BUFFER-filter->pending,processed;
    if(n>left)n=left;
    if(!n)return 0;
    memcpy(filter->buffer+filter->pending,input,n);
    filter->pending+=n;input+=n;left-=n;
    processed=filterConvert(filter);
    if(processed>filter->pending)return 0;
    if(processed)
    {
      if(ISeqOutStream_Write(filter->next,filter->buffer,processed)!=processed)return 0;
      filter->pc+=(UInt32)processed;
      filter->pending-=processed;
      memmove(filter->buffer,filter->buffer+processed,filter->pending);
    }
  }
  return size;
}

static SRes filterFinish(FilterStream *filter)
{
  /* Incomplete final instructions are literal bytes. Earlier stages must
     finish first so their tails still pass through every following filter. */
  if(filter->pending&&ISeqOutStream_Write(filter->next,filter->buffer,filter->pending)!=filter->pending)
    return SZ_ERROR_WRITE;
  filter->pending=0;
  return SZ_OK;
}

typedef struct
{
  ISeqOutStream iface;
  ISeqOutStreamPtr next;
  UInt32 crc;
} CheckedStream;

static size_t checkedWrite(ISeqOutStreamPtr stream,const void *data,size_t size)
{
  CheckedStream *checked=(CheckedStream *)stream;
  size_t written=ISeqOutStream_Write(checked->next,data,size);
  if(written<=size)checked->crc=CrcUpdate(checked->crc,data,written);
  return written;
}
