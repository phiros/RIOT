#include <stddef.h>
#include <string.h>
#include <generic_ringbuffer.h>


void grb_ringbuffer_init(generic_ringbuffer_t *rb, char *buffer, unsigned int max_entries, size_t entry_size) {
	rb->buffer = buffer;
	rb->buffer_size = max_entries;
	rb->entry_size = entry_size;
	rb->next = 0;
	rb->free = max_entries;
}

int grb_add_element(generic_ringbuffer_t *rb, void *entry) {
	int pos = rb->next;
	char *buffer_pos = &rb->buffer[rb->entry_size * rb->next];
	memcpy(buffer_pos, entry, rb->entry_size); // << beware: here be dragons...
	if(rb->free > 0) rb->free--;
	if(rb->next == rb->buffer_size -1) rb->next = 0; // << wrap around
	else rb->next++;
	return pos;
}

int grb_add_elements(generic_ringbuffer_t *rb, void *array,
		unsigned int num) {
	char *chararray = array;
	for(unsigned int i=0; i<num; i++) {
		grb_add_element(rb, &chararray[i* rb->entry_size]);
	}
	return 0;
}

int grb_get_element(generic_ringbuffer_t *rb, void **entry, unsigned int index) {
	*entry = &rb->buffer[index * rb->entry_size];
	return 0;
}

int grb_cp_element(generic_ringbuffer_t *rb, void *entry, unsigned int index) {
	memcpy(&(rb->buffer[index * rb->entry_size]), entry, rb->entry_size);
	return 0;
}

int grb_get_last_index(generic_ringbuffer_t *rb) {
	if(rb->free == 0) return rb->buffer_size - 1;
	return rb->next;
}
