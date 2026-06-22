#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void initStaticCircularQueue (StaticCircularQueue *q) {
    if (q == NULL) {
        printf("\nErro! O ponteiro da fila esta apontando para NULL");
        return;
    }

    q->start = 0;
    q->end = -1;
    q->total_elements = 0;
}