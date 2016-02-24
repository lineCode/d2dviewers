#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "global.h"

/*{{{*/
/* initialize buffer, call once before first getbits or showbits */

void Initialize_Buffer()
{
  ld->Incnt = 0;
  ld->Rdptr = ld->Rdbfr + 2048;
  ld->Rdmax = ld->Rdptr;

  ld->Bfr = 0;
  Flush_Buffer(0); /* fills valid data into bfr */
}
/*}}}*/
/*{{{*/
void Fill_Buffer()
{
  int Buffer_Level;

  Buffer_Level = read(ld->Infile,ld->Rdbfr,2048);
  ld->Rdptr = ld->Rdbfr;

  if (System_Stream_Flag)
    ld->Rdmax -= 2048;


  /* end of the bitstream file */
  if (Buffer_Level < 2048)
  {
    /* just to be safe */
    if (Buffer_Level < 0)
      Buffer_Level = 0;

    /* pad until the next to the next 32-bit word boundary */
    while (Buffer_Level & 3)
      ld->Rdbfr[Buffer_Level++] = 0;

  /* pad the buffer with sequence end codes */
    while (Buffer_Level < 2048)
    {
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>24;
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>16;
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE>>8;
      ld->Rdbfr[Buffer_Level++] = SEQUENCE_END_CODE&0xff;
    }
  }
}
/*}}}*/

/*{{{*/

/* MPEG-1 system layer demultiplexer */

int Get_Byte()
{
  while(ld->Rdptr >= ld->Rdbfr+2048)
  {
    read(ld->Infile,ld->Rdbfr,2048);
    ld->Rdptr -= 2048;
    ld->Rdmax -= 2048;
  }
  return *ld->Rdptr++;
}
/*}}}*/
/*{{{*/
/* extract a 16-bit word from the bitstream buffer */
int Get_Word()
{
  int Val;

  Val = Get_Byte();
  return (Val<<8) | Get_Byte();
}
/*}}}*/

/*{{{*/
/* return next n bits (right adjusted) without advancing */

unsigned int Show_Bits(N)
int N;
{
  return ld->Bfr >> (32-N);
}
/*}}}*/
/*{{{*/
/* return next bit (could be made faster than Get_Bits(1)) */

unsigned int Get_Bits1()
{
  return Get_Bits(1);
}
/*}}}*/

/*{{{*/
/* advance by n bits */

void Flush_Buffer(N)
int N;
{
  int Incnt;

  ld->Bfr <<= N;

  Incnt = ld->Incnt -= N;

  if (Incnt <= 24)
  {
    if (System_Stream_Flag && (ld->Rdptr >= ld->Rdmax-4))
    {
      do
      {
        if (ld->Rdptr >= ld->Rdmax)
          Next_Packet();
        ld->Bfr |= Get_Byte() << (24 - Incnt);
        Incnt += 8;
      }
      while (Incnt <= 24);
    }
    else if (ld->Rdptr < ld->Rdbfr+2044)
    {
      do
      {
        ld->Bfr |= *ld->Rdptr++ << (24 - Incnt);
        Incnt += 8;
      }
      while (Incnt <= 24);
    }
    else
    {
      do
      {
        if (ld->Rdptr >= ld->Rdbfr+2048)
          Fill_Buffer();
        ld->Bfr |= *ld->Rdptr++ << (24 - Incnt);
        Incnt += 8;
      }
      while (Incnt <= 24);
    }
    ld->Incnt = Incnt;
  }

}
/*}}}*/
/*{{{*/
/* return next n bits (right adjusted) */

unsigned int Get_Bits(N)
int N;
{
  unsigned int Val;

  Val = Show_Bits(N);
  Flush_Buffer(N);

  return Val;
}
/*}}}*/
