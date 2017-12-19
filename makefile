CC = /opt/android-toolchain.new/bin/arm-linux-androideabi-
#CC = /home/linux/bin/android-toolchain/bin/arm-linux-androideabi-
TARGET = player
CFLAGS = -I./include -L./lib -lm -lz -fdiagnostics-color=always -fPIE -pie
LIB = -lavformat -lavcodec -lavdevice -lavfilter -lavutil -lswresample -lswscale -lEGL -lGLESv2 -Wdeprecated-declarations
SRC_FILES = main.c

$(TARGET): $(SRC_FILES)
	$(CC)gcc -g -o $(TARGET) $(SRC_FILES) $(CFLAGS) $(LIB)

clean: 
	rm -rf $(TARGET)