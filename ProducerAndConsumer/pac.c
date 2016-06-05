#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct th_info {
    int id;
};

int buffer;
int empty = 1;
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_cond = PTHREAD_COND_INITIALIZER;

void make_product(int tid) {
    pthread_mutex_lock(&buffer_mutex);
    while (!empty) {
        pthread_cond_wait(&empty_cond, &buffer_mutex);
    }
    int product = rand() % 1000;
    buffer = product;
    empty = 0;
    printf("producer %d produce product %d\n", tid, product);
    pthread_cond_signal(&full_cond);
    pthread_mutex_unlock(&buffer_mutex);
}

void consume_product(int tid) {
    pthread_mutex_lock(&buffer_mutex);
    while (empty) {
        pthread_cond_wait(&full_cond, &buffer_mutex);
    }
    printf("consumer %d cosume product %d\n", tid, buffer);
    empty = 1;
    pthread_cond_signal(&empty_cond);
    pthread_mutex_unlock(&buffer_mutex);
}

void *producer(void *arg) {
    struct th_info *info = (struct th_info *)(arg);
    printf("producer %d start!\n", info->id);
    for (int i = 0; i < 10; i++) {
        make_product(info->id);
    }
    free(arg);
    return NULL;
}

void *consumer(void *arg) {
    pthread_detach(pthread_self());
    struct th_info *info = (struct th_info *)(arg);
    printf("consumer %d start!\n", info->id);
    while (1) {
        consume_product(info->id);
    }
    free(arg);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: ./lab1 nProducers nConsumers\n");
        return 1;
    }

    int nProducers = atoi(argv[1]);
    int nConsumers = atoi(argv[2]);

    if (!(nProducers > 0 && nConsumers > 0)) {
        fprintf(stderr,
                "nProducers and nConsumers must be greater than zero\n");
        return 2;
    }

    pthread_t *ths = malloc(sizeof(pthread_t) * nProducers);
    for (int i = 0; i < nProducers; i++) {
        struct th_info *info = malloc(sizeof(struct th_info));
        info->id = i;
        int err = pthread_create(ths + i, NULL, producer, info);
        if (err < 0) {
            fprintf(stderr, "pthread_create error: %s\n", strerror(errno));
            return 3;
        }
    }

    for (int i = 0; i < nConsumers; i++) {
        struct th_info *info = malloc(sizeof(struct th_info));
        info->id = i;
        pthread_t th;
        int err = pthread_create(&th, NULL, consumer, info);
        if (err < 0) {
            fprintf(stderr, "pthread_create error: %s\n", strerror(errno));
            return 4;
        }
    }

    for (int i = 0; i < nProducers; i++) {
        pthread_join(ths[i], NULL);
    }

    free(ths);
}
