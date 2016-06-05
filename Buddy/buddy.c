#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct block {
  size_t size;
  size_t start;
  int used;
  struct block *next;
  struct block *prev;
} block;

block *init_block() {
  block *ret = malloc(sizeof(block));
  ret->next = ret;
  ret->prev = ret;
  ret->used = 0;
  return ret;
}

void push(block *head, block *b) {
  assert(head && b);
  block *tail = head->prev;
  tail->next = b;
  b->prev = tail;
  b->next = head;
  head->prev = b;
}

void remove_block(block *b) {
  assert(b && b->prev && b->next);
  b->next->prev = b->prev;
  b->prev->next = b->next;
  b->next = b->prev = b;
}

int empty(block *head) {
  assert(head);
  return head == head->next;
}

size_t maxsize = 512;
block *headers[10];

size_t f(size_t n) {
  size_t ret = 0;
  while (n >>= 1) {
    ret++;
  }
  return ret;
}

block *split(block *b) {
  assert(b);
  remove_block(b);
  block *ret = init_block();
  ret->size = b->size / 2;
  ret->start = b->start;
  b->start = b->start + ret->size;
  b->size = ret->size;
  push(headers[f(ret->size)], ret);
  push(headers[f(b->size)], b);
  return ret;
}

void request(size_t size, int id) {
  assert(id > 0);
  printf("f = %lu\n", f(size));
  for (size_t off = f(size); off < 10; off++) {
    block *head = headers[off];
    block *p = head->next;

    for (; head != p; p = p->next) {
      if (p->used == 0) {
        while (p->size >= size * 2) {
          p = split(p);
        }
        p->used = id;
        return;
      }
    }
  }
  printf("request error!\n");
}

int is_buddy(block *a, block *b) {
  assert(a && b);
  if (a->start > b->start)
    return is_buddy(b, a);
  if (a->start + a->size == b->start && a->start % (a->size * 2) == 0)
    return 1;
  return 0;
}

block *merge_imp(block *a, block *b) {
  assert(a && b && is_buddy(a, b));
  if (a->start > b->start)
    return merge_imp(b, a);
  a->size *= 2;
  free(b);
  return a;
}

void merge(block *b) {
  assert(b && b->next && b->prev);
  size_t off = f(b->size);
  if (off >= 10)
    return;
  block *head = headers[off];
  for (block *p = head->next; p != head; p = p->next) {
    if (p != b && p->used == 0 && is_buddy(p, b)) {
      remove_block(p);
      remove_block(b);
      block *newb = merge_imp(p, b);
      newb->used = 0;
      push(headers[f(newb->size)], newb);
      merge(newb);
      return;
    }
  }
}

void freeb(size_t size, int id) {
  assert(id > 0 && size > 0);
  block *head = headers[f(size)];
  for (block *p = head->next; p != head; p = p->next) {
    if (p->used == id && p->size == size) {
      p->used = 0;
      merge(p);
      return;
    }
  }
  printf("freeb error!\n");
}

void init() {
  for (int i = 0; i < 10; i++) {
    headers[i] = init_block();
  }
  block *b = init_block();
  b->start = 0;
  b->size = 512;
  push(headers[9], b);
}

void display() {
  for (int i = 0; i < 10; i++) {
    printf("%d", i);
    block *head = headers[i];
    for (block *p = head->next; p != head; p = p->next) {
      printf("->%lu(%d)(%lu)", p->size, p->used, p->start);
    }
    printf("\n");
  }
}

int main(void) {
  int flag = 1;

  init();

  do {
    char order;
    int size;
    int id;
    printf("请输入命令:以空格相隔\n");
    scanf("%c%d%d", &order, &size, &id);
    if (order == 'r') {
      request(size, id);
    } else if (order == 'f') {
      freeb(size, id);
    } else {
      printf("error %c!\n", order);
    }
    display();
    // printf("是否继续:(1 继续 other 退出):\n");
    // scanf("%d", &flag);
    // getchar();
  } while (flag == 1);

  return 0;
}
