#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include "cs402.h"
#include "my402list.h"
#include <ctype.h>
#include <unistd.h>
#include <signal.h>




int line_num;
int num_packets = 20;
int p = 3;
int bucket_depth = 10;
int tokens_in_bucket = 0;
double lambda = 1;
double mu = 0.35;
double r = 1.5;
struct timeval inter_arrival_times;
FILE* fp;
char* file_name;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
struct timeval sys_startTime;
struct timeval sys_endTime;
double emulation_Runms;
int packet_thread_running;
int token_thread_running;
char mode = 'D';
sigset_t set;
int Exit_Command = FALSE;


double total_pack_interArr = 0;
double total_service_time = 0;
double total_time_Q1 = 0;
double total_time_Q2 = 0;
double total_time_S1 = 0;
double total_time_S2 = 0;
double total_time_system = 0;
double total_time_system2 = 0;


int tokens_generated = 0;
int tokens_dropped = 0;
int packets_generated = 0;
int packets_dropped = 0;
int packets_completed = 0;
int packets_removed = 0;


pthread_t packArrival_thread;
pthread_t tokDeposit_thread;
pthread_t server1_thread;
pthread_t server2_thread;
pthread_t signal_thread;


My402List * Queue1;
My402List * Queue2;

typedef struct{
    struct timeval Q1_arr_time;
    struct timeval Q1_dept_time;
    struct timeval Q2_arr_time;
    struct timeval Q2_dept_time;
    struct timeval service_arr_time;
    struct timeval service_dept_time;
    long int service_time;
    int packet_num;
    int token_req;
} Packet;

void verify_line(char* ptr){
   int string_length = strlen(ptr);

   if(ptr[string_length-1] != '\n'){
      printf("Error in line %d: Each line should be terminated with a new line character", line_num);
      exit(1);
   }

   if(ptr[0] == ' ' || ptr[0] == '\t' || ptr[string_length-2] == ' ' || ptr[string_length-2] == '\t'){
      printf("Error in line %d: There must be no leading or trailing space or tab characters in a line.\n", line_num);
      exit(1);
   } 
  
}

void populate_attributes(int * attr, char* ptr, int * i, char* attr_name){
   char* start_ptr;
   int start_ind = *i;

   while (isdigit(ptr[*i]))
   {
      (*i)++; 

   }

   if(ptr[*i] != ' ' && ptr[*i] != '\t' && ptr[*i] != '\n'){
      fprintf(stderr, "Error in Line %d: %s must be a positive integer\n", line_num, attr_name);
      exit(1);
   }

   if((strcmp(attr_name,"Service Time") == 0) || (strcmp(attr_name,"Number of Packets") == 0)){
      if(ptr[*i] != '\n'){
         fprintf(stderr, "Error in Line %d: Too many fields\n", line_num);
         exit(1);
      }
   }

   ptr[*i] = '\0';
   (*i)++;

     while((ptr[*i] == ' ' || ptr[*i] == '\t') && ptr[*i] != '\0') {
      (*i)++;
   }

   for(int j=0; j<start_ind; j++){
      ptr++;
   }

   start_ptr = ptr;


   *attr = atoi(start_ptr);

   if (*attr <= 0){
      printf("Error in Line %d: %s must be a positive integer\n", line_num, attr_name);
      exit(1);
   } 

}


void parse_line(int *inter_arr_time, int *tokens, int *service_time){
  
    char buffer[1025];
    fgets(buffer, sizeof(buffer), fp);
    line_num++;
    
    if(feof(fp)){
        fprintf(stderr, "Error (Too Less Entries): Number of packet entries should match the number of packets specified in first line of file.\n");
        exit(1);
    }

    if(buffer[strlen(buffer)-1] != '\n'){
        fprintf(stderr, "Error in Line %d: Incorrect Format\n", line_num);
        exit(1);
    }

    char* ptr = buffer;
    verify_line(ptr);

    int i=0;

    populate_attributes(inter_arr_time, ptr, &i, "Inter Arrival Time");
    populate_attributes(tokens, ptr, &i, "Tokens");
    populate_attributes(service_time, ptr, &i, "Service Time");

}

void readFirstLine(){

    char buffer[1025];
    fgets(buffer, sizeof(buffer), fp);
    line_num++;
    
    if(feof(fp)){
        fprintf(stderr, "Error: File \"%s\" is an empty file\n", file_name);
        exit(1);
    }

    if(buffer[strlen(buffer)-1] != '\n'){
        fprintf(stderr, "Error in Line %d: Incorrect Format\n", line_num);
        exit(1);
    }

    char* ptr = buffer;
    int i = 0; 
    
    verify_line(ptr);

    populate_attributes(&num_packets, ptr, &i, "Number of Packets");
}



double timeDiffCalc(struct timeval startTime, struct timeval endTime){
    double div = 1000;
    double result;
    result = (((endTime.tv_sec - startTime.tv_sec)*1000000L + endTime.tv_usec) - startTime.tv_usec)/div;    
    return result;
}

int thread_sleep_calc(struct timeval prev_tstamp, long int inter_arr_time_sec){
    struct timeval currtime;
    gettimeofday(&currtime, NULL);
    long int next_pack_arr_sec = prev_tstamp.tv_sec *1000000L + prev_tstamp.tv_usec + inter_arr_time_sec;
    long int elapsed_sec = currtime.tv_sec*1000000L + currtime.tv_usec;
    return (next_pack_arr_sec - elapsed_sec);
}

void generate_packet(int pack_num, Packet * pack, struct timeval *prev_tstamp){
    
    struct timeval pack_arrival_time;
    long int inter_pack_time_usec;
    int service_time_msec;
    long int sleep_time;

    if(mode == 'T'){
        int ts_inter_time;
        int ts_service_time;
        parse_line(&ts_inter_time, &p, &ts_service_time);
        inter_pack_time_usec = ts_inter_time*1000L;
        service_time_msec = ts_service_time;
    }
    
    if(mode=='D'){
        int int_pack_ms = round((1/lambda)*1000);

        if(int_pack_ms > 10*1000){
            int_pack_ms = 10*1000;
        }
        inter_pack_time_usec = (int_pack_ms)*1000L;

        service_time_msec = round((1/mu)*1000);

        if(service_time_msec > 10*1000){
            service_time_msec = 10*1000;
        }
    }

    sleep_time = thread_sleep_calc(*prev_tstamp, inter_pack_time_usec);


    if (sleep_time > 0 || Exit_Command == TRUE){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
        usleep(sleep_time);   
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

    }

    pack->packet_num = pack_num+1;
    pack->token_req = p;
    pack->service_time = service_time_msec;
    gettimeofday(&pack_arrival_time, NULL);
    double elapsedtime = timeDiffCalc(sys_startTime, pack_arrival_time);
    double inter_atime = timeDiffCalc(*prev_tstamp, pack_arrival_time);
    total_pack_interArr += inter_atime;
    *prev_tstamp = pack_arrival_time;
    pthread_mutex_lock(&mutex);
    packets_generated++;
    if (pack->token_req > bucket_depth){
        packets_dropped++;
        printf("%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms, dropped\n", elapsedtime, pack->packet_num, pack->token_req, inter_atime);      
    }
    else{
        printf("%012.3fms: p%d arrives, needs %d tokens, inter-arrival time = %.3fms\n", elapsedtime, pack->packet_num, pack->token_req, inter_atime);
    }
    pthread_mutex_unlock(&mutex);      
}

void transfer_packet(){
    double elapsedtime;
    My402ListElem * ptr = My402ListFirst(Queue1);
    Packet * packet = ptr->obj;
    tokens_in_bucket -= packet->token_req;
    My402ListUnlink(Queue1, ptr);
    gettimeofday(&(packet->Q1_dept_time), NULL);
    elapsedtime = timeDiffCalc(sys_startTime, packet->Q1_dept_time);
    double time_in_Q1 = timeDiffCalc(packet->Q1_arr_time, packet->Q1_dept_time);
    if (tokens_in_bucket == 0){
        printf("%012.3fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d token\n", elapsedtime, packet->packet_num, time_in_Q1, tokens_in_bucket);
    }
    else{
        printf("%012.3fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d tokens\n", elapsedtime, packet->packet_num, time_in_Q1, tokens_in_bucket);
    }
    if (My402ListEmpty(Queue2)){
        pthread_cond_broadcast(&cv);
    }
    My402ListAppend(Queue2, packet);
    gettimeofday(&(packet->Q2_arr_time), NULL);   
    elapsedtime = timeDiffCalc(sys_startTime, packet->Q2_arr_time);       
    printf("%012.3fms: p%d enters Q2\n", elapsedtime, packet->packet_num);
}


void * process_packets (void* arg){

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    struct timeval prev_tstamp = sys_startTime;
    double elapsedtime;

    for(int i = 0; i < num_packets; i++){
        Packet* packet = (Packet *)malloc(sizeof(Packet));
        generate_packet(i, packet, &prev_tstamp);   
        if (packet->token_req > bucket_depth){
            free(packet);
        }
        else{
            pthread_mutex_lock(&mutex);
            My402ListAppend(Queue1, packet);
            gettimeofday(&(packet->Q1_arr_time), NULL);
            elapsedtime = timeDiffCalc(sys_startTime, packet->Q1_arr_time);
            printf("%012.3fms: p%d enters Q1\n", elapsedtime, packet->packet_num);
            if((My402ListLength(Queue1) - 1 == 0) && (packet->token_req <= tokens_in_bucket)){
                transfer_packet();
            }
            pthread_mutex_unlock(&mutex);
        }
    }


    if(mode == 'T'){
        getc(fp);
        if(!feof(fp)){
            pthread_mutex_lock(&mutex);
            fprintf(stderr, "Error (Too Many Entries): Number of packet entries should match the number of packets specified in first line of file.\n");
            pthread_mutex_unlock(&mutex);
            exit(1);
        }

        fclose(fp);
    }

    
    pthread_mutex_lock(&mutex);
    packet_thread_running = FALSE;
    pthread_mutex_unlock(&mutex);
    return (0);
}

void generate_token(struct timeval *prev_tstamp, int * stop){


    struct timeval token_gen_time;
    int int_tok_ms = round((1/r)*1000);
    if(int_tok_ms > 10*1000){
        int_tok_ms = 10*1000;
    }
    long int inter_tok_time = (int_tok_ms)*1000L;
    long int sleep_time = thread_sleep_calc(*prev_tstamp, inter_tok_time);
    
    if (sleep_time > 0 || Exit_Command == TRUE){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
        usleep(sleep_time);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    }


    gettimeofday(&token_gen_time, NULL);
    double elapsedtime = timeDiffCalc(sys_startTime, token_gen_time);
    *prev_tstamp = token_gen_time;
    pthread_mutex_lock(&mutex);

    if(((packet_thread_running == FALSE) && My402ListEmpty(Queue1))){
        *stop = TRUE;
    }
    else
    {   
        tokens_generated++;
        if(tokens_in_bucket < bucket_depth){
            tokens_in_bucket++;
            if (tokens_in_bucket == 1){
                printf("%012.3fms: token t%d arrives, token bucket now has %d token\n", elapsedtime, tokens_generated, tokens_in_bucket);
            }
            else {
                printf("%012.3fms: token t%d arrives, token bucket now has %d tokens\n", elapsedtime, tokens_generated, tokens_in_bucket);                
            }
        }
        else{
            tokens_dropped++;
            printf("%012.3fms: token t%d arrives, dropped\n", elapsedtime, tokens_generated);                
        }
    }

    pthread_mutex_unlock(&mutex);    
}


void * deposit_tokens(void * arg){

    struct timeval prev_tstamp = sys_startTime;
    tokens_generated = 0;
    tokens_dropped = 0;
    int stop = FALSE;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    while(!stop){


        generate_token(&prev_tstamp, &stop);

        if(!stop){
            pthread_mutex_lock(&mutex);
            if(!My402ListEmpty(Queue1)){
                My402ListElem * elem = My402ListFirst(Queue1);
                Packet * packet = elem->obj;
                if (packet->token_req <= tokens_in_bucket){
                    transfer_packet(); 
                }
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    pthread_mutex_lock(&mutex);
    token_thread_running = FALSE;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mutex);    
    return (0);
}


Packet * dequeue_packet(){
    double elapsedtime;
    double time_in_Q2;
    My402ListElem * elem = My402ListFirst(Queue2);    
    Packet * pack = elem->obj;
    My402ListUnlink(Queue2, elem);
    gettimeofday(&(pack->Q2_dept_time), NULL);
    elapsedtime = timeDiffCalc(sys_startTime, pack->Q2_dept_time);
    time_in_Q2 = timeDiffCalc(pack->Q2_arr_time, pack->Q2_dept_time);
    printf("%012.3fms: p%d leaves Q2, time in Q2 = %.3fms\n", elapsedtime, pack->packet_num, time_in_Q2);
    return pack;    
}


void service_packet(Packet * packet, int server_num){
    double elapsedtime;
    gettimeofday(&(packet->service_arr_time), NULL);
    elapsedtime = timeDiffCalc(sys_startTime, packet->service_arr_time);
    printf("%012.3fms: p%d begins service at S%d, requesting %ldms of service\n", elapsedtime, packet->packet_num, server_num, packet->service_time);
}


void dispatch_packet(Packet* packet, int server_num){
    if(packet->packet_num == num_packets){
        packet_thread_running = FALSE;
        token_thread_running = FALSE;
        pthread_cond_broadcast(&cv);
    }
    double elapsedtime;
    double service_time;
    double time_in_system;
    gettimeofday(&(packet->service_dept_time), NULL);
    elapsedtime = timeDiffCalc(sys_startTime, packet->service_dept_time);
    service_time = timeDiffCalc(packet->service_arr_time, packet->service_dept_time);
    total_service_time += service_time;
    time_in_system = timeDiffCalc(packet->Q1_arr_time, packet->service_dept_time);
    printf("%012.3fms: p%d departs from S%d, service time = %.3fms, time in system = %.3fms\n", elapsedtime, packet->packet_num, server_num, service_time, time_in_system);
}

void store_stats(Packet* packet, int server_num){

    total_time_Q1 += timeDiffCalc(packet->Q1_arr_time, packet->Q1_dept_time);
    total_time_Q2 += timeDiffCalc(packet->Q2_arr_time, packet->Q2_dept_time);

    if(server_num == 1){
        total_time_S1 += timeDiffCalc(packet->service_arr_time, packet->service_dept_time);
    }

    if(server_num == 2){
        total_time_S2 += timeDiffCalc(packet->service_arr_time, packet->service_dept_time);
    }
    double pack_sys_time = (timeDiffCalc(packet->Q1_arr_time, packet->service_dept_time));
    total_time_system += pack_sys_time;
    total_time_system2 += pow(pack_sys_time,2);
}


void * server(void * arg){    
    int server_num = (int) arg;
    int stop = FALSE;
    Packet* packet;

    while(!stop){
        pthread_mutex_lock(&mutex);
        while(My402ListEmpty(Queue2)){
            if(((packet_thread_running == FALSE) && (token_thread_running == FALSE)) || Exit_Command == TRUE){
                stop = TRUE;
                break;
            }
            pthread_cond_wait(&cv, &mutex);
        }

        if(!stop){
            packet = dequeue_packet();
            service_packet(packet, server_num);
        }
        pthread_mutex_unlock(&mutex);

        if(!stop){
            usleep((packet->service_time)*1000);
            pthread_mutex_lock(&mutex);
            dispatch_packet(packet, server_num);
            packets_completed++;
            store_stats(packet, server_num);
            pthread_mutex_unlock(&mutex);
        }
    }
    pthread_exit(0);
}

void * monitor(){

    int sig;
    sigwait(&set, &sig);
    pthread_mutex_lock(&mutex);
    pthread_cancel(packArrival_thread);
    pthread_cancel(tokDeposit_thread);
    Exit_Command = TRUE;
    fflush(stdout);
    printf("SIGINT caught, no new packets or tokens will be allowed\n");
    pthread_cond_broadcast(&cv);

    while(!My402ListEmpty(Queue1)){
        double elapsedtime;
        struct timeval currtime;
        My402ListElem * elem = My402ListFirst(Queue1);
        Packet* packet = elem->obj;
        My402ListUnlink(Queue1, elem);
        gettimeofday(&(currtime), NULL);
        elapsedtime = timeDiffCalc(sys_startTime, currtime);
        printf("%012.3fms: p%d removed from Q1\n", elapsedtime, packet->packet_num);
        packets_removed++;
    }

        while(!My402ListEmpty(Queue2)){
        double elapsedtime;
        struct timeval currtime;
        My402ListElem * elem = My402ListFirst(Queue2);
        Packet* packet = elem->obj;
        My402ListUnlink(Queue2, elem);
        gettimeofday(&(currtime), NULL);
        elapsedtime = timeDiffCalc(sys_startTime, currtime);
        printf("%012.3fms: p%d removed from Q2\n", elapsedtime, packet->packet_num);
        packets_removed++;
    }

    pthread_mutex_unlock(&mutex);
    return (0);
}


static void malformed_alert(){
    fprintf(stderr, "Invalid Command!\n");
    fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
    exit(1);
}

void print_emul_param(){
    printf("Emulation Parameters: \n");
    printf("\tnumber to arrive = %d\n", num_packets);
    if(mode == 'D'){
        printf("\tlambda = %.6g\n", lambda);
        printf("\tmu = %.6g\n", mu);
    }
    printf("\tr = %.6g\n", r);
    printf("\tB = %d\n", bucket_depth);

    if(mode == 'D'){
        printf("\tP = %d\n", p);
    }

    if(mode == 'T'){
        printf("\ttsfile = %s\n", file_name);
    }

    printf("\n");
}

void print_stats(){

    double packet_drop_prob;
    double token_drop_prob;
    double avg_pack_interArr;
    double avg_service_time;
    double avg_packs_Q1;
    double avg_packs_Q2;
    double avg_packs_S1;
    double avg_packs_S2;
    double avg_time_sys;
    double avg_time_sys2;
    double stdDev;

    printf("\n");
    printf("Statistics: \n");
    printf("\n");

    if(packets_generated == 0){
        printf("\taverage packet inter-arrival time = N/A (No packets arrived)\n");
    }
    else{
        avg_pack_interArr = (total_pack_interArr/packets_generated)/1000;
        printf("\taverage packet inter-arrival time = %.6g\n", avg_pack_interArr);    
    }

    if(packets_completed == 0){
        printf("\taverage packet service time = N/A (No packets served)\n");
    }
    else{
        avg_service_time = (total_service_time/packets_completed)/1000;
        printf("\taverage packet service time = %.6g\n", avg_service_time);    
    }

    printf("\n");

    if(emulation_Runms == 0){
        printf("\taverage number of packets in Q1 = N/A (No packets served)\n");    
    }
    else{
        avg_packs_Q1 = total_time_Q1/emulation_Runms;
        printf("\taverage number of packets in Q1 = %.6g\n", avg_packs_Q1);
    }

    if(emulation_Runms == 0){
        printf("\taverage number of packets in Q2 = N/A (No packets served)\n");    
    }    
    else{
        avg_packs_Q2 = total_time_Q2/emulation_Runms;
        printf("\taverage number of packets in Q2 = %.6g\n", avg_packs_Q2);    
    }

    if(emulation_Runms == 0){
        printf("\taverage number of packets in S1 = N/A (No packets served)\n");        
    }
    else{
        avg_packs_S1 = total_time_S1/emulation_Runms;
        printf("\taverage number of packets in S1 = %.6g\n", avg_packs_S1);    
    }

    if(emulation_Runms == 0){
        printf("\taverage number of packets in S2 = N/A (No packets served)\n");    
    }
    else{
        avg_packs_S2 = total_time_S2/emulation_Runms;
        printf("\taverage number of packets in S2 = %.6g\n", avg_packs_S2);    
    }


    printf("\n");

    if(packets_completed == 0){
        printf("\taverage time a packet spent in system = N/A (No packets served)\n");
    }
    else{
        avg_time_sys = (total_time_system/packets_completed);
        printf("\taverage time a packet spent in system = %.6g\n", avg_time_sys/1000);    
    }

    if(packets_completed == 0){
        printf("\tstandard deviation for time spent in system =  N/A (No packets served)\n");
    }
    else{
        avg_time_sys2 = (total_time_system2/packets_completed);
        stdDev = sqrt(avg_time_sys2 - pow(avg_time_sys,2));
        printf("\tstandard deviation for time spent in system =  %.6g\n", stdDev/1000);    
    }

    printf("\n");


    if(tokens_generated == 0){
        printf("\ttoken drop probability = N/A (No tokens produced)\n");
    }
    else{
        token_drop_prob = (double)tokens_dropped/tokens_generated;
        printf("\ttoken drop probability = %.6g\n", token_drop_prob);
    }

    if(packets_generated == 0){
        printf("\tpacket drop probability = N/A (No packets produced)\n");
    }
    else{
        packet_drop_prob = (double)packets_dropped/packets_generated;
        printf("\tpacket drop probability = %.6g\n", packet_drop_prob);    
    }


}


void processCommands(int argc, char* argv[]){
    if (argc%2 == 0){
        malformed_alert();
        exit(1);
    }

    fp = NULL;

    for (int i=1; i<argc; i=i+2){
        if(strcmp(argv[i],"-lambda") == 0){
            lambda = atof(argv[i+1]);
            if(lambda <= 0){
                fprintf(stderr, "Input Error: lambda must be positive real number.\n");
                exit(1);
            }                        
            
        }else if(strcmp(argv[i],"-mu") == 0){
            mu = atof(argv[i+1]);
            if(mu <= 0){
                fprintf(stderr, "Input Error: mu must be positive real number.\n");
                exit(1);
            }                        
        }else if(strcmp(argv[i],"-r") == 0){
            r = atof(argv[i+1]);
            if(r <= 0){
                fprintf(stderr, "Input Error: r must be positive real number.\n");
                exit(1);
            }
        }else if(strcmp(argv[i],"-B") == 0){
            bucket_depth = atoi(argv[i+1]);
            if(bucket_depth <= 0){
                fprintf(stderr, "Input Error: B must be positive integer with a maximum value of 2147483647 (0x7fffffff)\n");
                exit(1);
            }
        }else if(strcmp(argv[i],"-P") == 0){
            p = atoi(argv[i+1]);
            if(p <= 0){
                fprintf(stderr, "Input Error: P must be positive integer with a maximum value of 2147483647 (0x7fffffff)\n");
                exit(1);
            }
        }else if(strcmp(argv[i],"-n") == 0){
            num_packets = atoi(argv[i+1]);
            if(num_packets <= 0){
                fprintf(stderr, "Input Error: num must be positive integer with a maximum value of 2147483647 (0x7fffffff)\n");
                exit(1);
            }
        }else if(strcmp(argv[i],"-t") == 0){
            fp = fopen(argv[i+1], "r");
            file_name = argv[i+1];
            

            if(fp == NULL){
                fprintf(stderr,"Error while opening Input File \"%s\": ", file_name);
                perror("");
                exit(1);
            }
            mode = 'T';
            readFirstLine();
        }else{
            malformed_alert();
        }        
    }
                            
}

int main(int argc, char* argv[])
{
    int error;
    processCommands(argc, argv);
    print_emul_param();


    fprintf(stdout, "00000000.000ms: emulation begins\n");
    
    Queue1 = (My402List*)malloc(sizeof(My402List));
    Queue2 = (My402List*)malloc(sizeof(My402List));

    My402ListInit(Queue1);
    My402ListInit(Queue2);

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, 0);
    

    gettimeofday(&sys_startTime, NULL);

    if ((error = pthread_create(&packArrival_thread, NULL, process_packets, NULL))){
        pthread_mutex_lock(&mutex);
        fprintf(stderr, "Packet arrival thread creation failed: %s\n", strerror(error));
        pthread_mutex_unlock(&mutex);
    }

    packet_thread_running = TRUE;

    if ((error = pthread_create(&tokDeposit_thread, NULL, deposit_tokens, NULL))){
        pthread_mutex_lock(&mutex);
        fprintf(stderr, "Token deopsit thread creation failed: %s\n", strerror(error));
        pthread_mutex_unlock(&mutex);
    }

    token_thread_running = TRUE;

    if ((error = pthread_create(&server1_thread, NULL, server, (void*)1))){
        pthread_mutex_lock(&mutex);
        fprintf(stderr, "Server1 thread creation failed: %s\n", strerror(error));
        pthread_mutex_unlock(&mutex);
    }

    if ((error = pthread_create(&server2_thread, NULL, server, (void*)2))){
        pthread_mutex_lock(&mutex);
        fprintf(stderr, "Server2 thread creation failed: %s\n", strerror(error));
        pthread_mutex_unlock(&mutex);
    }

    if((error = pthread_create(&signal_thread, 0, monitor, NULL))){
        pthread_mutex_lock(&mutex);
        fprintf(stderr, "Signal thread creation failed: %s\n", strerror(error));
        pthread_mutex_unlock(&mutex);
    }

    pthread_join(packArrival_thread, NULL);
    pthread_join(tokDeposit_thread, NULL);
    pthread_join(server1_thread, NULL);
    pthread_join(server2_thread, NULL);

    gettimeofday(&sys_endTime, NULL);
    emulation_Runms = timeDiffCalc(sys_startTime, sys_endTime);
    printf("%012.3fms: emulation ends\n", emulation_Runms);

    print_stats();

    exit(0);
}


