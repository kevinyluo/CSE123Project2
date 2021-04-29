#include "sender.h"

#include <assert.h>

#define WINDOW_SIZE 4;

static const MAX_SEQ = 255;

void init_sender(Sender* sender, int id) {
    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);
    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;
    
    sender->timeout_timeval = NULL;
    sender->buffer_framelist_head = NULL;
    sender->window_buffer_head = NULL;

    // Sliding window initialization
    sender->seqNum = MAX_SEQ;
    sender->SWS = WINDOW_SIZE;
    sender->LFS = MAX_SEQ;
    sender->LAR = MAX_SEQ;
}

struct timeval* sender_get_next_expiring_timeval(Sender* sender) {
    // No buffered msg => no expiration time
    // if (sender->pending_frame == NULL) {
    //     return NULL;
    // }
    if (sender->window_buffer_head == NULL || ll_get_length(sender->window_buffer_head) == 0) {
        return NULL;
    }

    // Expiration time in the next 0.09 seconds (90ms)
    struct timeval* exp_timeval = malloc(sizeof(struct timeval));
    gettimeofday(exp_timeval, NULL);
    exp_timeval->tv_usec += 90000;
    exp_timeval->tv_sec += 0;


    // Format time
    exp_timeval->tv_sec += exp_timeval->tv_usec / 1000000;
    exp_timeval->tv_usec %= 1000000;

    return exp_timeval;
}

void handle_incoming_acks(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    // If I received a msg from a receiver...
    int incoming_msgs_length = ll_get_length(sender->input_framelist_head);
    while (incoming_msgs_length > 0) {
        // Pop a node off the front of the link list and update the count
        LLnode* ll_inmsg_node = ll_pop_node(&sender->input_framelist_head);
        incoming_msgs_length = ll_get_length(sender->input_framelist_head);
        
        char* raw_char_buf = ll_inmsg_node->value;
        Frame* inframe = convert_char_to_frame(raw_char_buf);

        // Free raw_char_buf
        free(raw_char_buf);

        // If acknowledgement is for me..
        int length = ll_get_length(sender->window_buffer_head);

        uint8_t acceptable_seq = sender->LAR + 1;
        if (length > 0 && inframe->remainder == 0 && inframe->src_id == sender->send_id && inframe->seqNum == acceptable_seq) {
            ll_pop_node(&sender->window_buffer_head);
            sender->LAR++;
            // Clear timeout interval & buffer
            free(sender->timeout_timeval);
            sender->timeout_timeval = NULL;
        }

        free(inframe);
        free(ll_inmsg_node);
    }
}

void handle_input_cmds(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);

    // Recheck the command queue length to see if stdin_thread dumped a command
    // on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    while (input_cmd_length > 0) {

        // Pop a node off and update the input_cmd_length
        LLnode* ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        // Cast to Cmd type and free up the memory for the node
        Cmd* outgoing_cmd = (Cmd*) ll_input_cmd_node->value;
        free(ll_input_cmd_node);
        
        int msg_length = strlen(outgoing_cmd->message);

        if (msg_length > FRAME_PAYLOAD_SIZE) {
            // Parition the message if it is too large

            int i = 0;

            while(msg_length > FRAME_PAYLOAD_SIZE){
                Frame* outgoing_frame = calloc(1, sizeof(Frame));
                assert(outgoing_frame);
                outgoing_frame->msg_len = strlen(outgoing_cmd->message);
                outgoing_frame->seqNum = ++sender->seqNum;
                outgoing_frame->flags = msg_length == strlen(outgoing_cmd->message) ? 's' : 'c';  

                outgoing_frame->src_id = outgoing_cmd->src_id;
                outgoing_frame->dst_id = outgoing_cmd->dst_id;
                memcpy(outgoing_frame->data, outgoing_cmd->message + i, FRAME_PAYLOAD_SIZE);

                // Append frame to buffer
                ll_append_node(&sender->buffer_framelist_head, outgoing_frame);

                i += FRAME_PAYLOAD_SIZE;
                msg_length -= FRAME_PAYLOAD_SIZE;
            }

            // Send the last packet
            Frame* outgoing_frame = calloc(1, sizeof(Frame));
            assert(outgoing_frame);
            outgoing_frame->seqNum =  ++sender->seqNum;        
            outgoing_frame->flags = 'f';   
            outgoing_frame->src_id = outgoing_cmd->src_id;
            outgoing_frame->dst_id = outgoing_cmd->dst_id;
            memcpy(outgoing_frame->data, outgoing_cmd->message + i, msg_length);

            // At this point, we don't need the outgoing_cmd
            free(outgoing_cmd->message);
            free(outgoing_cmd);

            // Append frame to buffer
            ll_append_node(&sender->buffer_framelist_head, outgoing_frame);


        } else {
            // Queue packets
            // This is probably ONLY one step you want
            Frame* outgoing_frame = calloc(1, sizeof(Frame));
            assert(outgoing_frame);
            outgoing_frame->seqNum =  ++sender->seqNum;         
            outgoing_frame->flags = 'd';   
            outgoing_frame->src_id = outgoing_cmd->src_id;
            outgoing_frame->dst_id = outgoing_cmd->dst_id;
            strcpy(outgoing_frame->data, outgoing_cmd->message);

            // At this point, we don't need the outgoing_cmd
            free(outgoing_cmd->message);
            free(outgoing_cmd);

            // Append frame to buffer
            ll_append_node(&sender->buffer_framelist_head, outgoing_frame);

        }
    }

    // Send a packet
    int buffered_frames_length = ll_get_length(sender->buffer_framelist_head);
    if (buffered_frames_length > 0 && sender->LFS - sender->LAR <= sender->SWS) {
        LLnode* ll_frame_node = ll_pop_node(&sender->buffer_framelist_head);
        Frame* outgoing_frame = (Frame*) ll_frame_node->value;

        sender->LFS = outgoing_frame->seqNum;

        free(ll_frame_node);
    
        // Append the frame to the window buffer
        ll_append_node(&sender->window_buffer_head, outgoing_frame);

        // Convert the message to the outgoing_charbuf
        char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
        ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
    }
}


void handle_timedout_frames(Sender* sender, LLnode** outgoing_frames_head_ptr) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // printf("length is %d\n", ll_get_length(sender->window_buffer_head));

    // If there is a buffered message & we timed-out waiting for ACK
    if ((sender->timeout_timeval != NULL) && (timeval_usecdiff(&current_time, sender->timeout_timeval) <= 0)) {
        // Resend all packets in window
        int count = 0;
        for(int i = sender->LAR; i < sender->LFS; i++){
            LLnode* ll_frame_node = ll_get_node(&sender->window_buffer_head, count);
            Frame* outgoing_frame = (Frame*) ll_frame_node->value;

            // printf("attempting retransmit of %d\n", outgoing_frame->seqNum);
            char* outgoing_charbuf = convert_frame_to_char(outgoing_frame);
            ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);
            count++;
        }
    }
}

void* run_sender(void* input_sender) {
    struct timespec time_spec;
    struct timeval curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender* sender = (Sender*) input_sender;
    LLnode* outgoing_frames_head;
    struct timeval* expiring_timeval;
    long sleep_usec_time, sleep_sec_time;

    // This incomplete sender thread, at a high level, loops as follows:
    // 1. Determine the next time the thread should wake up
    // 2. Grab the mutex protecting the input_cmd/inframe queues
    // 3. Dequeues messages from the input queue and adds them to the
    // outgoing_frames list
    // 4. Releases the lock
    // 5. Sends out the messages

    while (1) {
        outgoing_frames_head = NULL;

        // Get the current time
        gettimeofday(&curr_timeval, NULL);

        // time_spec is a data structure used to specify when the thread should
        // wake up The time is specified as an ABSOLUTE (meaning, conceptually,
        // you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        // Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);
        if (sender->timeout_timeval != NULL) free(sender->timeout_timeval);
        sender->timeout_timeval = expiring_timeval;

        // Perform full on timeout
        if (expiring_timeval == NULL) {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        } else {
            // Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval, expiring_timeval);

            // Sleep if the difference is positive
            if (sleep_usec_time > 0) {
                sleep_sec_time = sleep_usec_time / 1000000;
                sleep_usec_time = sleep_usec_time % 1000000;
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time * 1000;
            }
        }

        // Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000) {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        // NOTE: Anything that involves dequeing from the input frames or input
        // commands should go
        //      between the mutex lock and unlock, because other threads
        //      CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        // Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);

        // Nothing (cmd nor incoming frame) has arrived, so do a timed wait on
        // the sender's condition variable (releases lock) A signal on the
        // condition variable will wakeup the thread and reaquire the lock
        if ((input_cmd_length == 0 && inframe_queue_length == 0) || sender->timeout_timeval != NULL) {
            pthread_cond_timedwait(&sender->buffer_cv, &sender->buffer_mutex,
                                   &time_spec);
        }
        // Implement this
        handle_incoming_acks(sender, &outgoing_frames_head);

        // Implement this
        handle_input_cmds(sender, &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);

        // Implement this
        handle_timedout_frames(sender, &outgoing_frames_head);

        // CHANGE THIS AT YOUR OWN RISK!
        // Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);

        while (ll_outgoing_frame_length > 0) {
            LLnode* ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char* char_buf = (char*) ll_outframe_node->value;

            // Frame* inframe = convert_char_to_frame(char_buf);
            // printf("sending %d\n", inframe->seqNum);

            // Don't worry about freeing the char_buf, the following function
            // does that
            send_msg_to_receivers(char_buf);

            // Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
