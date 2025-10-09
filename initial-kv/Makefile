SRC_DIR = src

kv: $(SRC_DIR)/kv.o $(SRC_DIR)/UniversalHash.o
	gcc -o kv $(SRC_DIR)/kv.o $(SRC_DIR)/UniversalHash.o -Wall

$(SRC_DIR)/kv.o: $(SRC_DIR)/kv.c $(SRC_DIR)/UniversalHash.h
	gcc -O -Wall -c $(SRC_DIR)/kv.c -o $(SRC_DIR)/kv.o

$(SRC_DIR)/UniversalHash.o: $(SRC_DIR)/UniversalHash.c $(SRC_DIR)/UniversalHash.h
	gcc -O -Wall -c $(SRC_DIR)/UniversalHash.c -o $(SRC_DIR)/UniversalHash.o

clean:
	rm -f $(SRC_DIR)/*.o kv