# mp4_scale
采用ffmepg来解析mp4文件为yuv数据，然后将yuv数据转化为jpeg格式图片保存。

ffmepg的编译采用neon方式编译，arm平台cpu会有专门的指令集优化，经过测试，采用neon方式比不采用，速度快了三倍左右，还是很有价值的。

ffmepg编译（3.3.2）：
	./configure --prefix=/home/linux/study/03.ffmpeg/arm-reduce-out --enable-static --disable-shared --enable-pthreads --enable-cross-compile \
	--cross-prefix=arm-linux-androideabi- --cc=arm-linux-androideabi-gcc --arch=arm  --target-os=linux --optflags=-O0 \
	--disable-programs --disable-encoders --enable-encoder=mjpeg \
	--disable-protocols --enable-protocol=file  \
	--disable-muxers --enable-muxer=mjpeg \
	--disable-demuxers --enable-demuxer=mov \
	--disable-outdevs --disable-indevs \
	--enable-neon \
	--extra-cflags="-DANDROID -mfloat-abi=softfp -mfpu=neon" \
	--enable-asm \
	--disable-yasm --disable-decoder=aac --disable-decoder=aac_latm

	由于打开asm的时候libavcodec/aacdec编译会出错，所以屏蔽掉aac以后编译通过。可以屏蔽libavcodec/aacdec.c中的 #   include "arm/aac.h"，然后打开aac也可以编译通过。
