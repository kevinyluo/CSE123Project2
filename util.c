#include "util.h"

// Linked list functions
int ll_get_length(LLnode* head) {
    LLnode* tmp;
    int count = 1;
    if (head == NULL)
        return 0;
    else {
        tmp = head->next;
        while (tmp != head) {
            count++;
            tmp = tmp->next;
        }
        return count;
    }
}

void ll_append_node(LLnode** head_ptr, void* value) {
    LLnode* prev_last_node;
    LLnode* new_node;
    LLnode* head;

    if (head_ptr == NULL) {
        return;
    }

    // Init the value pntr
    head = (*head_ptr);
    new_node = (LLnode*) malloc(sizeof(LLnode));
    new_node->value = value;

    // The list is empty, no node is currently present
    if (head == NULL) {
        (*head_ptr) = new_node;
        new_node->prev = new_node;
        new_node->next = new_node;
    } else {
        // Node exists by itself
        prev_last_node = head->prev;
        head->prev = new_node;
        prev_last_node->next = new_node;
        new_node->next = head;
        new_node->prev = prev_last_node;
    }
}

LLnode* ll_get_node(LLnode** head_ptr, int index) {
    LLnode* curr = (*head_ptr);

    int count = 0;
    while (curr != NULL) {
        if (count == index)
            return (curr);
        count++;
        curr = curr->next;
    }

    return NULL;
}


LLnode* ll_pop_node(LLnode** head_ptr) {
    LLnode* last_node;
    LLnode* new_head;
    LLnode* prev_head;

    prev_head = (*head_ptr);
    if (prev_head == NULL) {
        return NULL;
    }
    last_node = prev_head->prev;
    new_head = prev_head->next;

    // We are about to set the head ptr to nothing because there is only one
    // thing in list
    if (last_node == prev_head) {
        (*head_ptr) = NULL;
        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    } else {
        (*head_ptr) = new_head;
        last_node->next = new_head;
        new_head->prev = last_node;

        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
}

void ll_destroy_node(LLnode* node) {
    if (node->type == llt_string) {
        free((char*) node->value);
    }
    free(node);
}

// Compute the difference in usec for two timeval objects
long timeval_usecdiff(struct timeval* start_time, struct timeval* finish_time) {
    long usec;
    usec = (finish_time->tv_sec - start_time->tv_sec) * 1000000;
    usec += (finish_time->tv_usec - start_time->tv_usec);
    return usec;
}

// Print out messages entered by the user
void print_cmd(Cmd* cmd) {
    fprintf(stderr, "src=%d, dst=%d, message=%s\n", cmd->src_id, cmd->dst_id,
            cmd->message);
}

// Encrypt char buffer with CRC-32
void crc_encrypt(char* char_buf) {
    size_t frameSize = sizeof(Frame);
    char* remainder = malloc(frameSize);
    memcpy(remainder, char_buf, frameSize);

    // Calculate remainder (Divide).
    for (size_t i = 0; i < frameSize - 4; i++) {
        for (int j = 0; j < 8; j++) {
            if ((remainder[i] & (0x80 >> j)) == 0) continue;
            remainder[i] ^= ((CRC_GENERATOR >> (4 * 8 + j)) & 0xFF);
            remainder[i + 1] ^= ((CRC_GENERATOR >> (3 * 8 + j)) & 0xFF);
            remainder[i + 2] ^= ((CRC_GENERATOR >> (2 * 8 + j)) & 0xFF);
            remainder[i + 3] ^= ((CRC_GENERATOR >> (1 * 8 + j)) & 0xFF);
            remainder[i + 4] ^= ((CRC_GENERATOR >> j) & 0xFF);
        }
    }

    // Subtract remainder.
    for (size_t i = frameSize - 4; i < frameSize; i++) {
        char_buf[i] ^= remainder[i];
    }

    free(remainder);
}

// Decrypt char buffer with CRC-32
void crc_decrypt(char* char_buf) {
    size_t frameSize = sizeof(Frame);
    char* remainder = malloc(frameSize);
    memcpy(remainder, char_buf, frameSize);

    // Calculate remainder (Divide).
    for (size_t i = 0; i < frameSize - 4; i++) {
        for (int j = 0; j < 8; j++) {
            if ((remainder[i] & (0x80 >> j)) == 0) continue;
            remainder[i] ^= ((CRC_GENERATOR >> (4 * 8 + j)) & 0xFF);
            remainder[i + 1] ^= ((CRC_GENERATOR >> (3 * 8 + j)) & 0xFF);
            remainder[i + 2] ^= ((CRC_GENERATOR >> (2 * 8 + j)) & 0xFF);
            remainder[i + 3] ^= ((CRC_GENERATOR >> (1 * 8 + j)) & 0xFF);
            remainder[i + 4] ^= ((CRC_GENERATOR >> j) & 0xFF);
        }
    }

    // Set remainder.
    for (size_t i = frameSize - 4; i < frameSize; i++) {
        char_buf[i] = remainder[i];
    }

    free(remainder);
}

char* convert_frame_to_char(Frame* frame) {
    frame->remainder = 0;
    char* char_buffer = malloc(sizeof(Frame));
    memcpy(char_buffer, frame, sizeof(Frame));
    crc_encrypt(char_buffer);
    return char_buffer;
}

Frame* convert_char_to_frame(char* char_buf) {
    crc_decrypt(char_buf);
    Frame* frame = malloc(sizeof(Frame));
    memcpy(frame, char_buf, sizeof(Frame));
    return frame;
}