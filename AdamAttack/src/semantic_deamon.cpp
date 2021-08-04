#define SEMANTIC_BATCH_SIZE 256
#define BUFF_MAX_SIZE_STRING 50

int server_port = 16845;
const char* server_host = "127.0.0.1"; 

void* semantic_provider(void *arg){
    struct threadArgs *t = (struct threadArgs *)arg;
    
    // init socket-------------------------------------------
    int sock, bs;
    socklen_t optlen;
    int optval;
    int loc_status = EXIT_FAILURE;
    struct sockaddr_in addr_in;
    
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(server_port);
    inet_pton(addr_in.sin_family, server_host, &addr_in.sin_addr.s_addr);
    sock = socket(addr_in.sin_family, SOCK_STREAM,0);
    if( sock == -1){
        fprintf(stderr,"[ERROR]: unable to create socket\n");
        pthread_exit(&loc_status);
    }
    
    if(getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0) {
        fprintf(stderr,"[ERROR]: unable to check KEEPALIVE option on socket\n");
        close(sock);
        pthread_exit(&loc_status);
    }
    
    if( !optval){
        /* Set the option active */
        optval = 1;
        optlen = sizeof(optval);
        if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
            fprintf(stderr,"[ERROR]: unable to set KEEPALIVE option on socket\n");
            close(sock);
            pthread_exit(&loc_status);
        }
    }
        
    int connect_retries = 0;
    while(true){
        if( -1 != connect(sock,(struct sockaddr*)&addr_in, sizeof(addr_in))){
            break;
        }
        fprintf(stderr,"[WARN]: unable to connect to %s on port %d\n", server_host, server_port);
        connect_retries++;
        sleep(SLEEP_T);
        if(connect_retries >= MAX_CONNECT_RETRY){
            fprintf(stderr,"[ERROR]: unable to connect to %s on port %d after %d retries. Shutting down the application\n", server_host, server_port, connect_retries);
            pthread_exit(&loc_status);
        }
    }
    //-------------------------------------------------------
    
    wordlist_t *matched_list = t->matched_list_ptr;
    wordlist_t *wordlist = t->wordlist;
#ifdef HITS_SOURCE_TRACING
    std::list<int> *source_wordlist = t->source_wordlist;
#endif
    
    uint64_t ib;
    
    char *batch_string = (char*) Malloc(sizeof(char) * (BUFF_MAX_SIZE_STRING+1) * SEMANTIC_BATCH_SIZE, "Batch string\n");
    
    fprintf(stderr, "[LOG] semantic_provider started\n");
    
    while(true){
        pthread_mutex_lock(&matched_list_mutex);


        // wait for SEMANTIC_BATCH_SIZE+ words in matched_list
        while(matched_list->size() < SEMANTIC_BATCH_SIZE){
            pthread_cond_wait(&cond_available_in_matched_list, &matched_list_mutex);
        }
        
        // consume matched_list and fill a batch to send to the daemon
        char *_batch_string = batch_string;
        size_t batch_string_len = 0;
        for(ib = 0; ib < SEMANTIC_BATCH_SIZE; ib++){
            //fprintf(stderr, "\t%lu]-------->%s\n", matched_list->size(), matched_list->front());
            const char *s = matched_list->front();
            //batch_lens[ib] = strlen(s);
            sprintf(_batch_string, "%s\t", s);
            
            int len = strlen(s);
            _batch_string += len + 1;
            batch_string_len += len + 1;
            matched_list->pop_front();
        }
        
        //remove last \t
        batch_string[batch_string_len-1] = '\0';
        //fprintf(stderr, "%s\n", batch_string);

        // stop reading the matched list
        pthread_mutex_unlock(&matched_list_mutex);
        
        // comunication with deamon --------------------------------------
        // send
        bs = Write(sock, (void*) &batch_string_len, sizeof(size_t));
        //fprintf(stderr,"[DT] Sent %lu\n",data_len_w);
        bs = Write(sock, (void*) batch_string, batch_string_len);
        //receive 
        size_t batch_string_len_rec = 0;
        bs = Read(sock, &batch_string_len_rec, sizeof(size_t));
        
        if(batch_string_len_rec > 0){
            // todo pre-allocate this
            char *new_words = (char *)Malloc((batch_string_len_rec+1), "Malloc new_words");
            bs = Read(sock, new_words, batch_string_len_rec);
            char *token = strtok(new_words, WORD_BATCH_DELIMITER_STRING);
            
            // insert words into the wordlist
            pthread_mutex_lock(&word_list_mutex);
            
            while(token){
                char *tmp = strdup(token);
                //fprintf(stderr, "\t-------->%s\n", tmp);

                wordlist->push_back(tmp);
#ifdef HITS_SOURCE_TRACING
                source_wordlist->push_back(SOURCE_SEM);
#endif
                token = strtok(NULL, WORD_BATCH_DELIMITER_STRING);
            }
            pthread_mutex_unlock(&word_list_mutex);
            free(new_words);
        }
        
        
        // check exit condition
        pthread_mutex_lock(&matched_list_mutex);
        bool exit_cond = MAIN_ATTACK_STOP && (matched_list->size() < SEMANTIC_BATCH_SIZE);
        pthread_mutex_unlock(&matched_list_mutex);
        if(exit_cond){
                fprintf(stderr,"[LOG] Semantic consumer thread terminated\n");
            goto END;
        }
    }
    
END:
    free(batch_string);
}