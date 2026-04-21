CC=gcc
CFLAGS=-ffast-math -O3 -g -Wall -march=native -funroll-loops -fno-omit-frame-pointer
OPTFLAGS=-ffast-math -O3 -g -Wall -march=native -funroll-loops -fno-omit-frame-pointer
OBJS_COMMON=kernel.o rdtsc.o

all:	check calibrate measure

check:	$(OBJS_COMMON) -lm driver_check.o
	$(CC) -o $@ $^
calibrate: $(OBJS_COMMON) -lm driver_calib.o
	$(CC) -o $@ $^
measure: $(OBJS_COMMON) -lm driver_measure.o
	$(CC) -o $@ $^

driver_check.o: src/driver_check.c
	$(CC) $(CFLAGS) -D CHECK -c $< -o $@
driver_calib.o: src/driver_calib.c
	$(CC) $(CFLAGS) -D CALIB -c $< -o $@
driver_measure.o: src/driver_measure.c
	$(CC) $(CFLAGS) -c $<

kernel.o: src/kernel.c
	$(CC) $(OPTFLAGS) -D $(OPT) -c $< -o $@

rdtsc.o: src/rdtsc.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS_COMMON) driver_check.o driver_calib.o driver_measure.o check calibrate measure