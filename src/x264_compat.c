/* Provides x264_bit_depth missing from conda videoprocess libx264.so.138.
   libavcodec.so.56 requires this symbol at load time even for decode-only use. */
int x264_bit_depth = 8;
