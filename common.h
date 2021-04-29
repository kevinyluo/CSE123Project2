#ifndef __COMMON_H__
#define __COMMON_H__

#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
typedef unsigned char uchar_t;

// System configuration information
struct SysConfig_t {
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t SysConfig;

// Command line input information
struct Cmd_t {
    uint16_t src_id;
    uint16_t dst_id;
    char* message;
};
typedef struct Cmd_t Cmd;

// Linked list information
enum LLtype { llt_string, llt_frame, llt_integer, llt_head } LLtype;

struct LLnode_t {
    struct LLnode_t* prev;
    struct LLnode_t* next;
    enum LLtype type;

    void* value;
};
typedef struct LLnode_t LLnode;

#define MAX_FRAME_SIZE 64

// TODO: You should change this!
// Remember, your frame can be AT MOST 64 bytes!
// In project 1, the FRAME_PAYLOAD_SIZE should be larger than 48!
#define FRAME_PAYLOAD_SIZE 48
#define CRC_SIZE 4
#define CRC_GENERATOR 0x82608EDB80
// #define CRC_GENERATOR 0b1000001001100000100011101101101110000000
struct Frame_t {
    unsigned char flags;            // 1
    uint8_t seqNum;                 // 1 
    uint16_t src_id;                // 2
    uint16_t dst_id;                // 2
    uint32_t msg_len;               // 4
    char data[FRAME_PAYLOAD_SIZE];  // 48
    uint32_t remainder;             // 4
};
typedef struct Frame_t Frame;

// Receiver and sender data structures
struct Receiver_t {
    // DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_framelist_head;
    int recv_id;
    uint16_t* sender_seq_ids;
    // Sliding Window Variables
    uint8_t RWS;
    uint8_t LAF;
    uint8_t LFR;
    char* long_msg;
};

struct Sender_t {
    // DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode* input_cmdlist_head;
    LLnode* input_framelist_head;
    int send_id;
    Frame* pending_frame;
    struct timeval* timeout_timeval;
    uint8_t seqNum;
    uint16_t packet_id;
    LLnode* buffer_framelist_head;
    // Sliding Window Variables
    LLnode* window_buffer_head;
    uint8_t SWS;
    uint8_t LFS;
    uint8_t LAR;
};

enum SendFrame_DstType { ReceiverDst, SenderDst } SendFrame_DstType;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;

// Declare global variables here
// DO NOT CHANGE:
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender* glb_senders_array;
Receiver* glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;

#endif
