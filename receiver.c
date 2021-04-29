#include "receiver.h"

#define WINDOW_SIZE 1;

static const MAX_SEQ = 255;

void init_receiver(Receiver* receiver, int id) {
    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;

    // Track sequences for each sender
    receiver->sender_seq_ids = calloc(glb_senders_array_length, sizeof(uint16_t));

    receiver->RWS = WINDOW_SIZE;
    receiver->LAF = WINDOW_SIZE - 1;
    receiver->LFR = MAX_SEQ;
    receiver->long_msg = NULL;
}

void handle_incoming_msgs(Receiver* receiver,
                          LLnode** outgoing_frames_head_ptr) {
    // TODO: Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is for this receiver
    //    4) Acknowledge that this frame was received

    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
    while (incoming_msgs_length > 0) {
        // Pop a node off the front of the link list and update the count
        LLnode* ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

        char* raw_char_buf = ll_inmsg_node->value;
        Frame* inframe = convert_char_to_frame(raw_char_buf);

        // Free raw_char_buf
        free(raw_char_buf);
        

        // If message is for me and it is within the frame
        if (inframe->remainder == 0 && inframe->dst_id == receiver->recv_id && (inframe->seqNum > receiver->LFR || (inframe->seqNum == 0 && receiver->LFR ==MAX_SEQ )) && inframe->seqNum <= receiver->LAF ) {
            // Print (if not already received)
            uint8_t packet_next_id = (uint8_t) (inframe->seqNum + 1);

            // Update sliding window
            receiver->LFR = inframe->seqNum;
            receiver->LAF = receiver->RWS + receiver->LFR ;


            
            if (packet_next_id > receiver->sender_seq_ids[inframe->src_id] || (packet_next_id == 0 &&  receiver->LAF == 0 )) {
                if(inframe->flags == 's'){
                    receiver->long_msg = malloc(inframe->msg_len);
                    memcpy(receiver->long_msg, inframe->data, FRAME_PAYLOAD_SIZE);

                }
                else if (inframe->flags == 'c'){
                    int len = strlen(receiver->long_msg);
                    memcpy(receiver->long_msg + len, inframe->data, FRAME_PAYLOAD_SIZE);
                }
                else if (inframe->flags == 'f'){
                    int len = strlen(receiver->long_msg);
                    memcpy(receiver->long_msg + len, inframe->data, strlen(inframe->data));
                    printf("<RECV_%d>:[%s]\n", receiver->recv_id, receiver->long_msg);
                }
                else{
                    printf("<RECV_%d>:[%s]\n", receiver->recv_id, inframe->data);
                }
                receiver->sender_seq_ids[inframe->src_id] = packet_next_id;
            }
        }
        else if(inframe->remainder == 0 && inframe->dst_id != receiver->recv_id && (inframe->seqNum > receiver->LFR || (inframe->seqNum == 0 && receiver->LFR ==MAX_SEQ )) && inframe->seqNum <= receiver->LAF ){
            // Update sliding window
            receiver->LFR = inframe->seqNum;
            receiver->LAF = receiver->RWS + receiver->LFR ;
        }

        // Send acknowledgement.
        Frame* outgoing_frame = calloc(1, sizeof(Frame));
        memcpy(outgoing_frame, inframe, sizeof(Frame));
        outgoing_frame->flags = 'a';

        char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
        ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
        free(outgoing_frame);

        free(inframe);
        free(ll_inmsg_node);
    }
}

void* run_receiver(void* input_receiver) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver* receiver = (Receiver*) input_receiver;
    LLnode* outgoing_frames_head;

    // This incomplete receiver thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up if there is nothing
    // in the incoming queue(s)
    // 2. Grab the mutex protecting the input_msg queue
    // 3. Dequeues messages from the input_msg queue and prints them
    // 4. Releases the lock
    // 5. Sends out any outgoing messages

    while (1) {
        // NOTE: Add outgoing messages to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval, NULL);

        // Either timeout or get woken up because you've received a datagram
        // NOTE: You don't really need to do anything here, but it might be
        // useful for debugging purposes to have the receivers periodically
        // wakeup and print info
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames should go
        //      between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        // Check whether anything arrived
        int incoming_msgs_length =
            ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0) {
            // Nothing has arrived, do a timed wait on the condition variable
            // (which releases the mutex). Again, you don't really need to do
            // the timed wait. A signal on the condition variable will wake up
            // the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv,
                                   &receiver->buffer_mutex, &time_spec);
        }

        handle_incoming_msgs(receiver, &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);

        // CHANGE THIS AT YOUR OWN RISK!
        // Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = (char*) ll_outframe_node->value;

            // The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);

            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
}
