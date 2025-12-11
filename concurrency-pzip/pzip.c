#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CHUNK_SIZE      (8ULL * 1024 * 1024)
#define QUEUE_CAPACITY  32
#define N_ENCODERS      4

typedef struct {
    size_t seq_id;
    uint8_t* data;
    size_t len;
    bool is_last;
} chunk_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    chunk_t* items;
    size_t capacity;
    size_t fillptr;
    size_t useptr;
    size_t length;
} mpmc_queue_t;

static void queue_init(mpmc_queue_t *q, size_t capacity) {
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    q->capacity = capacity;
    q->fillptr = q->useptr = q->length = 0;
    q->items = calloc(capacity, sizeof(chunk_t));
}

static void queue_destroy(mpmc_queue_t *q) {
    free(q->items);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

static void queue_push(mpmc_queue_t *q, chunk_t chunk) {
    pthread_mutex_lock(&q->mutex);
    while (q->length == q->capacity)
        pthread_cond_wait(&q->not_full, &q->mutex);

    q->items[q->fillptr] = chunk;
    q->fillptr = (q->fillptr + 1) % q->capacity;
    q->length++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

static chunk_t queue_pop(mpmc_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->length == 0)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    chunk_t chunk = q->items[q->useptr];
    q->useptr = (q->useptr + 1) % q->capacity;
    q->length--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return chunk;
}

void* reader(void*);
void* writer(void*);
void* encoder(void*);
void rle_encode(const uint8_t* in, size_t in_len, uint8_t** out_ptr, size_t* out_len);

mpmc_queue_t raw_queue;
mpmc_queue_t encoded_queue;

uint8_t *input_data;
size_t input_size;

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input> <output.pzip>\n", argv[0]);
        return 1;
    }

    char* input_file = argv[1];
    char* output_file = argv[2];

    int fd = open(input_file, O_RDONLY);
    if (fd < 0) {
        perror("open input");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        return 1;
    }

    input_size = st.st_size;
    if (input_size == 0) {
        printf("Empty file\n");
        close(fd);
        return 0;
    }

    input_data = mmap(NULL, input_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (input_data == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    close(fd);

    int out_fd = open(output_file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("open output");
        return 1;
    }

    queue_init(&raw_queue, QUEUE_CAPACITY);
    queue_init(&encoded_queue, QUEUE_CAPACITY);

    if (input_size < 1024*1024) {
        printf("pzip: %s (%.2f KB) → %s\n",
                   input_file,
                   input_size / 1024.0,
                   output_file);
    } else if (input_size < 1024*1024*1024) {
        printf("pzip: %s (%.2f MB) → %s\n",
                   input_file,
                   input_size / (1024.0*1024.0),
                   output_file);
    } else {
        printf("pzip: %s (%.2f GiB) → %s\n",
                   input_file,
                   input_size / (1024.0*1024.0*1024.0),
                   output_file);
    }

    pthread_t reader_thread, writer_thread, encoder_threads[N_ENCODERS];

    pthread_create(&reader_thread, NULL, reader, NULL);
    pthread_create(&writer_thread, NULL, writer, &out_fd);
    for (int i = 0; i < N_ENCODERS; i++) {
        pthread_create(&encoder_threads[i], NULL, encoder, NULL);
    }

    pthread_join(reader_thread, NULL);
    pthread_join(writer_thread, NULL);
    for (int i = 0; i < N_ENCODERS; i++) {
        pthread_join(encoder_threads[i], NULL);
    }

    close(out_fd);
    munmap(input_data, input_size);
    queue_destroy(&raw_queue);
    queue_destroy(&encoded_queue);

    printf("pzip: done! Compressed %lu bytes → %s\n", input_size, output_file);

    return 0;
}

void* reader(void* arg) {
    (void)arg;

    size_t seq_id = 0;
    size_t offset = 0;

    while (offset < input_size) {
        size_t this_chunk_size = CHUNK_SIZE;

        if (offset + this_chunk_size > input_size) {
            this_chunk_size = input_size - offset;
        }

        chunk_t chunk = {
            .seq_id  = seq_id,
            .data    = input_data + offset,
            .len     = this_chunk_size,
            .is_last = false
        };

        queue_push(&raw_queue, chunk);

        seq_id++;
        offset += this_chunk_size;
    }

    for (int i = 0; i < N_ENCODERS; i++) {
        chunk_t poison = {
            .is_last = true
        };
        queue_push(&raw_queue, poison);
    }

    printf("Reader: finished producing %lu chunks\n", seq_id);
    return NULL;
}

ssize_t write_full(int fd, const uint8_t* buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t written = write(fd, buf + total, count - total);
        if (written < 0) {
            if (errno == EINTR)
                continue;  // Interrupted
            return -1;
        }
        if (written == 0) {
            errno = EIO;
            return -1;
        }
        total += written;
    }
    return total;
}

void* writer(void* arg) {
    int out_fd = *(int*)arg;
    size_t next_expected = 0;
    int poison_count = 0;

    // ooo: out of order
    const int MAX_OOO = 32;
    chunk_t ooo_buffer[MAX_OOO];
    int ooo_size = 0;

    while (poison_count < N_ENCODERS) {
        chunk_t chunk = queue_pop(&encoded_queue);

        if (chunk.is_last) {
            poison_count++;
            continue;
        }

        if (chunk.seq_id != next_expected) {
            if (ooo_size >= MAX_OOO) {
                fprintf(stderr, "FATAL: out-of-order buffer overflow!\n");
                exit(1);
            }
            ooo_buffer[ooo_size++] = chunk;
            continue;
        }

        if (write_full(out_fd, chunk.data, chunk.len) < 0) {
            perror("write failed");
            exit(1);
        }
        free(chunk.data);
        next_expected++;

        bool drained = true;
        while (drained) {
            drained = false;
            for (int i = 0; i < ooo_size; i++) {
                if (ooo_buffer[i].seq_id == next_expected) {
                    if (write_full(out_fd, ooo_buffer[i].data, ooo_buffer[i].len) < 0) {
                        perror("write failed");
                        exit(1);
                    }
                    free(ooo_buffer[i].data);
                    next_expected++;
                    ooo_buffer[i] = ooo_buffer[--ooo_size];
                    drained = true;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < ooo_size; i++) {
        if (write_full(out_fd, ooo_buffer[i].data, ooo_buffer[i].len) < 0) {
            perror("write failed");
            exit(1);
        }
        free(ooo_buffer[i].data);
    }

    printf("Writer: finished — output written successfully\n");
    return NULL;
}

void* encoder(void* arg) {
    (void)arg;

    while (true) {
        chunk_t raw = queue_pop(&raw_queue);

        if (raw.is_last) {
            queue_push(&encoded_queue, raw);
            break;
        }

        uint8_t* encoded_data;
        size_t encoded_len;

        rle_encode(raw.data, raw.len, &encoded_data, &encoded_len);

        chunk_t encoded = {
            .seq_id = raw.seq_id,
            .data = encoded_data,
            .len = encoded_len,
            .is_last = false
        };

        queue_push(&encoded_queue, encoded);
    }

    return NULL;
}

void rle_encode(const uint8_t* in, size_t in_len, uint8_t** out_ptr, size_t* out_len) {
    uint8_t* out = malloc(in_len * 5);
    size_t pos = 0;

    size_t i = 0;
    while (i < in_len) {
        uint8_t current_char = in[i];
        size_t counter = 1;
        i++;

        while (i < in_len && in[i] == current_char && counter < UINT32_MAX) {
            i++;
            counter++;
        }

        uint32_t cnt = (uint32_t)counter;
        out[pos++] = cnt >> 0;
        out[pos++] = cnt >> 8;
        out[pos++] = cnt >> 16;
        out[pos++] = cnt >> 24;
        out[pos++] = current_char;
    }

    out = realloc(out, pos);
    *out_ptr = out;
    *out_len = pos;
}
