#include "../common/cMpeg2dec.h"

class cMyMpeg2dec : public cMpeg2dec {
public:
  //{{{
  void writeFrame (unsigned char*src[], int frame,
                    bool progressive, int coded_Picture_Width, int coded_Picture_Height, int chroma_Width) {
    if (progressive)
      printf ("writeFrame prog %d, %d %d\n", frame,coded_Picture_Width, coded_Picture_Height);
    else
      printf ("writeFrame interlaced %d, %d %d\n", frame,coded_Picture_Width, coded_Picture_Height);

    unsigned char* ptr = src[0];
    for (int j = 0; j < coded_Picture_Height; j++) {
      for (int i = 0; i < coded_Picture_Width; i++) {
        printf ("%1x", (*ptr++) >> 4);
        }
      printf ("\n");
      }
    }
  //}}}
  };

int main (int argc, char* argv[]) {

  cMyMpeg2dec mpeg2dec;

  mpeg2dec.Initialize_Buffer (argv[1]);
  mpeg2dec.Decode_Bitstream();


  return 0;
  }
