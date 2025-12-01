INCL = -I.
BUILD_NUMBER_FILE = build.no
RELEASE_NUMBER_FILE = release.no
BUILD_DATE=$$(date +'%Y-%m-%d %H:%M %z %Z')
BUILD_NUMBER=$$(cat $(BUILD_NUMBER_FILE))
RELEASE_NUMBER=$$(cat $(RELEASE_NUMBER_FILE))
CFLAGS = -DBUILD_NUMBER="\"$(BUILD_NUMBER)\"" -DBUILD_DATE="\"$(BUILD_DATE)\"" -DRELEASE_NUMBER="\"$(RELEASE_NUMBER)\"" -O3 -Wall $(INCL)
UNAME = $(shell uname)
CC = gcc
CPP = g++
LD = g++
LDFLAGS = -lpthread
TARGET = carith
TARGET_OBJS = main.o rle.o carith.o cbit.o color_print.o crc32.o
TEST_TARGET = algo_test
TEST_TARGET_OBJS = algo_test.o
RLEINT_TARGET = rleint
RLEINT_TARGET_OBJS = rleint.o rle.o carith.o cbit.o
LZSS_TEST_TARGET = lzss
LZSS_TEST_TARGET_OBJS = lzss_test.o

all: command test

command: $(TARGET)

test: $(TEST_TARGET) $(RLEINT_TARGET) $(LZSS_TEST_TARGET)

$(TARGET): $(TARGET_OBJS)

	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)
	$(LD) $(TARGET_OBJS) -o $(TARGET) $(LDFLAGS)

$(TEST_TARGET): $(TEST_TARGET_OBJS)

	$(LD) $(TEST_TARGET_OBJS) -o $(TEST_TARGET) $(LDFLAGS)

$(RLEINT_TARGET): $(RLEINT_TARGET_OBJS)

	$(LD) $(RLEINT_TARGET_OBJS) -o $(RLEINT_TARGET) $(LDFLAGS)

$(LZSS_TEST_TARGET): $(LZSS_TEST_TARGET_OBJS)

	$(LD) $(LZSS_TEST_TARGET_OBJS) -o $(LZSS_TEST_TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.cc
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -f *.o
	rm -f *~
	rm -f $(TARGET)
	rm -f $(TEST_TARGET)
	rm -f $(RLEINT_TARGET)
