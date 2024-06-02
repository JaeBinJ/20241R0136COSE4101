#include <stdio.h>
#include <stdlib.h> 
#include <time.h> 
#include <string.h>

#define PROC_NUM 6

#define FCFS 0
#define SJF 1
#define PRIORITY 2
#define RR 3
#define SRTF 4
#define PRE_PRIOR 5


typedef struct {
    int pid;
    int cpu_time;
    int io_time; //io요청 후 기다리는 시간
    int arrival_time;
    int io_start_time; //io작업이 시작되는 시간(cpu_burst_time 2초 진행 후로 고정)
    int prior;
    int cpu_remain_time; //for update
    int io_remain_time; //for update
    int waiting_time; //for update
    int turnaround_time;  //for update
    int new_arrive_time; //for update, rr
    int remain_time_quantum; //for update,  rr
} process;

typedef struct{
    float average_wait;
    float average_turn;
} eval;


void *Create_Process();
void Config();
void Schedule(int method);
void Evaluation(int method);
void make_Gantt(process * running, int time);
void print_Gantt(int time);
void init_Gantt();
process * algo(int method, process * prev_running);
void print_processes();


process * readyqueue[PROC_NUM];
process * waitingqueue[PROC_NUM];
process * processes[PROC_NUM] = {0, }; //단순 생성된 프로세스들의 목록
eval eval_res[6]; //평가 값들을 저장하는 배열, 인덱스 = 알고리즘



int main()
{
    srand(time(NULL));

    for (int i = 0; i < PROC_NUM; i++)
        Create_Process();
    
    print_processes();

    //각 알고리즘에 대해 schedule실행
    for (int i = 0; i<6; i++)
    {
        switch(i)
        {
            case FCFS:
                printf("-------------------------<FCFS>------------------------\n");
                break;
            case SJF:
                printf("-------------------------<SJF>------------------------\n");
                break;
            case PRIORITY:
                printf("-------------------------<PRIORITY>------------------------\n");
                break;
            case RR:
                printf("-------------------------<RR>------------------------\n");
                break;
            case SRTF:
                printf("-------------------------<SRTF>------------------------\n");
                break;
            case PRE_PRIOR:
                printf("-------------------------<PRE_PRIOR>------------------------\n");
                break;
        }
        Schedule(i);
        //eval_res에 저장된 값들을 출력
        printf("average turnaround time: %f, average waiting time: %f\n\n\n\n", eval_res[i].average_turn, eval_res[i].average_wait);    
    }    
}

void print_processes(){
    printf("\n<processes>\n");
    printf("=====================================================================\n");
    printf("pid           | ");
    for(int i=0; i<PROC_NUM; i++)
        printf("%d\t", processes[i]->pid);
    printf("\ncpu burst time|");
    for(int i=0; i<PROC_NUM; i++)
        printf("%d\t", processes[i]->cpu_time);
    printf("\nio burst time |");
    for(int i=0; i<PROC_NUM; i++)
        printf("%d\t", processes[i]->io_time);
    printf("\narrival time  |");
    for(int i=0; i<PROC_NUM; i++)
        printf("%d\t", processes[i]->arrival_time);
    printf("\npriority      |");   
    for(int i=0; i<PROC_NUM; i++)
        printf("%d\t", processes[i]->prior);
    printf("\n=====================================================================\n\n\n");
}

void * Create_Process(){
    //실행할 프로세스를 생성, 각 프로세스에 랜덤 데이터 생성
    //pid = 1 ~ 
    //CPU burst: 5 ~ 20
    //IO burst: 1 ~ 10
    //arival time : 0~PROC_NUM 
    //io_start_time: arrival 2만큼 후로 고정 (그렇지 않으면 arrivaltime보다 먼저iostart가 되는 경우가 발생)
    //priority : 1 ~ PROC_NUM
    process *new;
    new = (process *) malloc(sizeof(process));
    memset(new, 0, sizeof(process));

    new->pid = rand() % PROC_NUM + 1;
    new->cpu_time = (rand() % 16) + 5 ;
    new->io_time = rand() % 10+1;
    new->arrival_time = rand() % (PROC_NUM+1);
    new->io_start_time = 2;
    new->prior = (rand() % PROC_NUM) + 1;
    new->cpu_remain_time = new->cpu_time;
    new->io_remain_time = new->io_time;
    new->new_arrive_time = new->arrival_time;
    new->remain_time_quantum = 4; //기본 time quantum = 4

    while(1) //pid가 겹치지 않도록 하기 위함
    {
        if(processes[new->pid -1] == NULL)
        {
            processes[new->pid - 1] = new;
            break;}
        new -> pid = rand() % PROC_NUM + 1;
    }

    return new;

}

void Config(){
    //시스템 환경 설정 (ready queue, waiting queue)
    //생성된 프로세스에 맞게 각 큐를 초기화
    //각 스케줄링 시마다 초기에 실행
    for(int i = 0; i< PROC_NUM; i++)
    {
        waitingqueue[i]=NULL;
        readyqueue[i]=NULL;
        processes[i]->cpu_remain_time = processes[i]->cpu_time;
        processes[i]->io_remain_time = processes[i]->io_time;
        processes[i]->new_arrive_time = processes[i]->arrival_time;
        processes[i]->turnaround_time = 0;
        processes[i]->waiting_time = 0;
        processes[i]->remain_time_quantum = 4;
    }
}

void count_waiting(process *running) //수행 시 당시에 readyqueue에 있는 proc만 대상으로 waitingtime을 1증가시킨다
{
    for(int i = 0; i< PROC_NUM; i++)
    {
        if (readyqueue[i] && (readyqueue[i]->pid != running->pid)) //현재 실행 중인 프로세스는 제외
            (readyqueue[i])->waiting_time += 1;
    }
}

//큐 관리 시=> pop할 때는그 부분을 null로 바꿀 것. for문으로 null인지 아닌지 여부로 판단. 큐의 idx = pid-1임.
void Schedule(int method){
    //매초마다 확인

    process * running = NULL;
    process * io_running = NULL;
    int running_proc_num = PROC_NUM;
    int time = 0;

    Config(); //큐 초기화
    init_Gantt();

    while(1) //while문 1회 수행 당 
    {
        //running_proc_num이 0될때까지 while문 수행
        if(running_proc_num <= 0)
            break;


        //현재 타임에 실행이 가능하다면 ready queue로 옮기기
        for (int i=0; i < PROC_NUM; i++) //arrive proc을 일단 waiting queue에 넣기
        {
            if(processes[i]->arrival_time == time) 
                readyqueue[processes[i]->pid - 1] = processes[i];
        }

        //io 시작 시간에 도달한 proc(실행 2s가 지난 proc)을 ready queue에서 iowaitingqueue로 옮기기
        if(running)
        {
            if(running->cpu_time - running->cpu_remain_time == 2)
            {
                waitingqueue[running->pid-1] = running;
                readyqueue[running->pid - 1] = NULL;
                running = NULL;
            }
        }


        //ready queue에서 현재 수행할 proc정하기 => ready queue안에서 결정
        //여기에서 알고리즘을 도입 => 만약 없을 땐 null
        running = algo(method, running);
       
        //io처리중인 proc정하기 => waiting queue안에서 결정(pid순으로 실행한다고 가정), 없다면 null
        for(int i = 0; i < PROC_NUM; i++)
        {
            if(io_running)
                break;
            
            if(waitingqueue[i])
            {
                io_running = waitingqueue[i];
                break;
            }
        }
        
        //간트차트관리
        make_Gantt(running, time);

        //time 1 증가
        time += 1;

        //ready queue에 있는 p들 대상으로만 waiting time 증가시키기
        count_waiting(running);

        //실행중인 proc의 remaining time 1 감소
        if(running)
        {
                   
            running->cpu_remain_time -= 1;
            if(running->cpu_remain_time <= 0)//complete된 proc 확인
            {
                //있다면 time - arrive로 turnaround 계산 및 running_proc_num감소시키기, readyqueue에서 삭제
                running->turnaround_time = time - running->arrival_time;
                running_proc_num -= 1;
                readyqueue[running->pid - 1] = NULL;
                running = NULL;
            }
            else if(method == RR) //for rr
            {
                //for rr => 실행중인 프로세스의 remin time quantum 1감소, 
                //만약 remaintime quantum이 0이라면 running을 null로 바꿔주고 ready큐에 다시 넣고 new_arrivetime을 현재 시간으로 갱신
                running->remain_time_quantum -= 1;
                if(running->remain_time_quantum <= 0)
                {
                    running->new_arrive_time = time;
                    running = NULL;
                }
            }

        }

        //io처리 중인 proc의 io remaining time 1 감소
        if(io_running)
        {
            io_running->io_remain_time -= 1;
            if(io_running->io_remain_time <= 0) //complete여부 확인
            {
                //complete라면 ready큐로 옮기기
                readyqueue[io_running->pid - 1] = io_running;
                waitingqueue[io_running->pid -1] = NULL;
                io_running = NULL;
            }
        }
    }
    print_Gantt(time);
    Evaluation(method);
}


process *fcfs(process* prev_running) //prev=>nonpreemptive를 위해 도입  
{
    //ready큐 안에 존재하는 proc중 arrive time이 가장 작은 proc반환 (이미 실행중이던 것이 있으면 바로 그 proc반환)
    //없다면 NULL반환
    int i;
    int min;
    process * running = NULL;

    if(prev_running != NULL)
        return prev_running;

    for(i = 0; i < PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
            min = readyqueue[i]->arrival_time;
            running = readyqueue[i];
            break;
        }
    }

    if(running == NULL)
        return NULL;

    for(i = i+1; i<PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
           if(readyqueue[i]->arrival_time < min)
           {
                min = readyqueue[i]->arrival_time;
                running = readyqueue[i];
           }
        }
    }
    return running;
}

process *sjf(process* prev_running) //prev=>nonpreemptive를 위해 도입  
{
    //ready큐 안에 존재하는 proc중 cpu_time이 가장 작은 proc반환 (이미 실행중이던 것이 있으면 바로 그 proc반환)
    //없다면 NULL반환
    int i;
    int min;
    process * running = NULL;

    if(prev_running != NULL)
        return prev_running;

    for(i = 0; i < PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
            min = readyqueue[i]->cpu_time;
            running = readyqueue[i];
            break;
        }
    }

    if(running == NULL)
        return NULL;

    for(i = i+1; i<PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
           if(readyqueue[i]->cpu_time < min)
           {
                min = readyqueue[i]->cpu_time;
                running = readyqueue[i];
           }
        }
    }
    return running;

}

process * Priority(process* prev_running) //prev=>nonpreemptive를 위해 도입  
{
    //ready큐 안에 존재하는 proc중 priority이 가장 작은 proc반환 (이미 실행중이던 것이 있으면 바로 그 proc반환)
    //없다면 NULL반환
    int i;
    int min;
    process * running = NULL;

    if(prev_running != NULL)
        return prev_running;

    for(i = 0; i < PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
            min = readyqueue[i]->prior;
            running = readyqueue[i];
            break;
        }
    }

    if(running == NULL)
        return NULL;

    for(i = i+1; i<PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
           if(readyqueue[i]->prior < min)
           {
                min = readyqueue[i]->prior;
                running = readyqueue[i];
           }
        }
    }
    return running;
}

process * rr(process * prev_running)
{
    //ready 큐 안에 존재하는 proc중 new_arrivetime이 가장 작은 proc을 반환(일종의 큐 구현)
    //new_arrivetime = timequantum이 다 되어서 새로 레디큐에 들어가는 시간
    int i;
    int min;
    process * running = NULL;

    if(prev_running != NULL)
        return prev_running;
   

    for(i = 0; i < PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
            min = readyqueue[i]->new_arrive_time;
            running = readyqueue[i];
            break;
        }
    }

    if(running == NULL)
        return NULL;

    for(i = i+1; i<PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
           if(readyqueue[i]->new_arrive_time < min)
           {
                min = readyqueue[i]->new_arrive_time;
                running = readyqueue[i];
           }
        }
    }

    //새로운 프로세스 반환 시 remain time quantum은 4로 초기화되어야 할 것!!
    running->remain_time_quantum = 4;
    return running;

}

process *srtf()
{
    //ready큐 안에 존재하는 proc중 arrive time이 가장 작은 proc반환
    //실행가능한 것이 없다면 null반환
    int i;
    int min;
    process * running = NULL;

    for(i = 0; i < PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
            min = readyqueue[i]->cpu_remain_time;
            running = readyqueue[i];
            break;
        }
    }

    if(running == NULL)
        return NULL;

    for(i = i+1; i<PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
           if(readyqueue[i]->cpu_remain_time < min)
           {
                min = readyqueue[i]->cpu_remain_time;
                running = readyqueue[i];
           }
        }
    }
    return running;
}

process *pre_Prior()
{
    //ready큐 안에 존재하는 proc중 priority가 가장 작은 proc반환
    //실행가능한 것이 없다면 null반환
    int i;
    int min;
    process * running = NULL;

    for(i = 0; i < PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
            min = readyqueue[i]->prior;
            running = readyqueue[i];
            break;
        }
    }

    if(running == NULL)
        return NULL;

    for(i = i+1; i<PROC_NUM; i++)
    {
        if(readyqueue[i])
        {
           if(readyqueue[i]->prior < min)
           {
                min = readyqueue[i]->prior;
                running = readyqueue[i];
           }
        }
    }
    return running;
}

process * algo(int method, process * prev_running){
    //CPU 스케줄링 알고리즘을 구현
    process * running;
    switch (method)
    {
        case FCFS:
            running = fcfs(prev_running);
            break;
        case SJF:
            running = sjf(prev_running);
            break;
        case PRIORITY: 
            running = Priority(prev_running);
            break;
        case RR:
            running = rr(prev_running);
            break;
        case SRTF:
            running = srtf();
            break;
        case PRE_PRIOR:
            running = pre_Prior();   
            break;      
        default:
            break;
    }
    return running;
}


void Evaluation(int method){
    //각 스케줄링 알고리즘들간 비교 평가한다.
    //average 값을 구하기
    //processes배열에서 저장된 값들 기준으로 eval_res[method]에 update
    float turnaround = 0;
    float waiting = 0;

    for(int i = 0; i<PROC_NUM; i++)
    {
        turnaround += processes[i]->turnaround_time;
        waiting += processes[i]->waiting_time;
    }

    eval_res[method].average_turn = turnaround / (float)PROC_NUM;
    eval_res[method].average_wait = waiting / (float)PROC_NUM;
}

char Gantt_top[0x100];
char Gantt_content[0x100];
char Gantt_bottom[0x100];
char Gantt_time[0x100];


process * history = NULL;
void init_Gantt()
{
    memset(Gantt_top, 0, 0x100);
    memset(Gantt_content, 0, 0x100);
    memset(Gantt_bottom, 0, 0x100);
    memset(Gantt_time, 0, 0x100);
    strcat(Gantt_top, "=");
    strcat(Gantt_content, "|");
    strcat(Gantt_bottom, "=");
    strcat(Gantt_time, "0");

    history = NULL;
}


void make_Gantt(process * running, int time){
    //running중인 proc을 받아서 표기
    //만약 직전 proc이랑 pid랑 같으면 경계선이 없고 다르면 경계선이 o
    char new[3] = {0,};
    char new_time[3] = {0,};

    if(running == NULL) //실행 중인 프로세스가 없을 경우 (idle)
    {
        if(history == NULL) //직전 프로세스도 idle이었을 때
        {
            new[0] = 'X';
            strcat(Gantt_time, " ");}
        else{ //직전 프로세스는 idle이 아니었을 때
            new[0] = '|';
            new[1] = 'X';
            strcat(Gantt_top, "=");
            strcat(Gantt_bottom, "=");
            sprintf(new_time, "%d", time);
            strcat(Gantt_time, new_time);
            if(strlen(new_time)<=1) //만약 숫자가 두자리수라면 칸 수 맞춤
                strcat(Gantt_time, " ");
        }
    }
    else if (running == history) //실행중인 프로세스가 존재하며 직전 프로세스와 동일할 때
    {
        new[0] = ' ';
        strcat(Gantt_time, " ");
    }
    else //실행중인 프로세스가 존재하며 직전 프로세스와 동일하지 않을때
    {
        
        if(time==0) //가장 처음일때 경계선 표기 x(기본으로 있으므로)
        {
            new[0] =running->pid + 48;
            strcat(Gantt_time, " ");
        }
        else{ //가장 처음이 아니라면 경계선 표기
            new[0] = '|';
            new[1] = running->pid + 48;
            sprintf(new_time, "%d", time);
            strcat(Gantt_time, new_time);
            if(strlen(new_time)<=1)
                strcat(Gantt_time, " ");
            strcat(Gantt_top, "=");
            strcat(Gantt_bottom, "=");
        }
    }

    strcat(Gantt_top, "=");
    strcat(Gantt_content, new);
    strcat(Gantt_bottom, "=");

    history = running;
}

void print_Gantt(int time){
    char new_time[3];
    strcat(Gantt_top, "=");
    strcat(Gantt_content, "|");
    strcat(Gantt_bottom, "=");
    sprintf(new_time, "%d", time);
    strcat(Gantt_time, new_time);

    printf("<Gantt Chart>\n");
    printf("%s\n", Gantt_top);
    printf("%s\n", Gantt_content);
    printf("%s\n", Gantt_bottom);
    printf("%s\n", Gantt_time);
}