#ifndef __GENERIC_RINGBUFFER_H
#define __GENERIC_RINGBUFFER_H

typedef struct {
	char *buffer;
	unsigned int buffer_size;
	unsigned int entry_size;
	unsigned int next;
	unsigned int free;
} generic_ringbuffer_t;

void grb_ringbuffer_init(generic_ringbuffer_t *rb, char *buffer,
		unsigned int max_entries, size_t entry_size);

int grb_add_element(generic_ringbuffer_t *rb, void *entry);

int grb_add_elements(generic_ringbuffer_t *rb, void *array, unsigned int num);

int grb_get_element(generic_ringbuffer_t *rb, void **entry, unsigned int index);

int grb_cp_element(generic_ringbuffer_t *rb, void *entry, unsigned int index);

int grb_get_last_index(generic_ringbuffer_t *rb);

#endif /* __GENERIC_RINGBUFFER_H */
