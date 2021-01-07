#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <list>
using namespace std;

enum transitions{
    TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT, TRANS_TO_FINISH
};

enum schedulers{
    F, L, S, R, P, E
};

struct Process{
    int pid, AT, TC, CB, IO,
    PRIO, D_PRIO,
    remaining, burst_rem, state_ts, ST, burst_temp,
    FT, TT, IT, CW;
};

struct Event{
    int timestamp;
    transitions transition;
    Process* evtProcess;
};

ifstream file;
int ofs=0;
int quantum=1000000, maxprio=4;
int last_Process;
int current_time;

int IOT =0;
int IOE = 0;

vector<int> randvals;
vector<Process> processes;
schedulers schedule;
Process* current_running = nullptr;
Process* current_blocked = nullptr;

list<Process*>* activeL;
list<Process*>* expiredL;

class DES_layer{
    vector<Event> events;

public:
//    DES_layer(){
//        Event e;
//        events.push_back(e);
//    }

    Event get_event(){
        if (events.empty()){
            Event e;
            e.timestamp=-1;
            return e;
        }
        Event e = events.front();
        events.erase(events.begin());
        //cout<<events.size()<<endl;
        return e;
    }

    void put_event(Event e){
        int i =0;
        while (i < events.size() && e.timestamp >= events[i].timestamp) {
            i++;
        }
        events.insert(events.begin()+i,e);
    //        cout<<"events: "<<endl;
    //        for(int i=0; i<events.size(); i++){
    //            cout<<events[i].timestamp<<" "<<endl;
    //        }
    }
    int curNextTime(){
        //cout<<events.size()<<endl;
        for (int i=0; i<events.size();i++){
          //  cout<<" pid "<<events[i].evtProcess->pid<<endl;
            if(events[i].evtProcess->pid==current_running->pid){
                //cout<<"get cur next time"<<endl;
                return events[i].timestamp;
            }
        }
        return -1;
    }
    void removeCur() {
        for (int i=0; i<events.size();i++){
            if(events[i].evtProcess->pid==current_running->pid){
                events.erase(events.begin()+i);
              //  cout<<"remove"<<endl;
                return;
            }
        }
    }
    int nextTimestamp(){
        if (events.empty()) return -1;
        return events.front().timestamp;
    }

};

DES_layer des;

class Scheduler{
protected:
    list<Process*> runqueue;
public:
    virtual void add_to_runqueue(Process* p) = 0;
    virtual Process* get_next_process() = 0;
};

class FCFS: public Scheduler{
    void add_to_runqueue(Process *p) {
        runqueue.push_back(p);
//        for (list<Process*>::iterator it = runqueue.begin(); it != runqueue.end(); it++){
//            cout<<(*it)->pid<<endl;
//        }
        //cout<<"runqueue size "<<runqueue.size()<<endl;
    }
    Process * get_next_process() {
        Process *p;
        if(runqueue.empty()) {
//            cout<<"runqueue empty"<<endl;
            return nullptr;
        }
        p = runqueue.front();
        //cout<<p->pid<<endl;
        runqueue.erase(runqueue.begin());
//        cout<<"runqueue size-1 "<<runqueue.size()<<endl;
        return p;
    }
};

class LCFS: public Scheduler{
    void add_to_runqueue(Process *p) {
        runqueue.push_back(p);
    }
    Process * get_next_process() {
        Process *p;
        if(runqueue.empty()) return nullptr;
        p = runqueue.back();
        runqueue.pop_back();
        return p;
    }
};

class SRTF: public Scheduler{
    void add_to_runqueue(Process *p) {
        runqueue.push_back(p);
    }
    Process * get_next_process() {
        Process* p = runqueue.front();
        if(runqueue.empty()) return nullptr;
        for (list<Process*>::iterator it = ++runqueue.begin(); it != runqueue.end(); it++){
            if ((p->remaining) > (*it)->remaining){
                p = *it;
            }
        }
        runqueue.remove(p);
        return p;
    }
};

class RR : public Scheduler {
public:
    void add_to_runqueue(Process *p) {
        runqueue.push_back(p);
    }
    Process * get_next_process() {
        Process *p;
        if(runqueue.empty()) return nullptr;
        p = runqueue.front();
        runqueue.erase(runqueue.begin());
        return p;
    }
};

class PRIO : public Scheduler {
public:
    void add_to_runqueue(Process *p) {
        //cout<<"add"<<endl;
        if (p->D_PRIO<0){
            p->D_PRIO = p->PRIO-1;
            expiredL[p->D_PRIO].push_back(p);
           // cout<<"add expired "<<p->D_PRIO<<endl;
        }
        else {
            activeL[p->D_PRIO].push_back(p);
           // cout<<"add active "<<p->D_PRIO<<endl;
        }


    }
    Process * get_next_process() {
        Process *p;
        //cout<<"get"<<endl;
        for (int i = maxprio-1; i>=0; i--){
            if (!activeL[i].empty()){
                //cout<<i<<endl;
                p = activeL[i].front();
                activeL[i].pop_front();
                return p;
            }
        }
        list<Process*>* temp;
        temp = activeL;
        activeL = expiredL;
        expiredL= temp;
        for (int i = maxprio-1; i>=0; i--){
            if (!activeL[i].empty()){
                p = activeL[i].front();
                activeL[i].pop_front();
                //cout<<p->pid<<endl;
                return p;
            }
        }
        return nullptr;
    }
};
//
//PRIO::PRIO(list<Process*> active, list<Process*> expired){
//    activeL = &active;
//    expiredL = &expired;
//}

class PREPRIO : public Scheduler {
public:
    void add_to_runqueue(Process *p) {
        if (p->D_PRIO<0){
            p->D_PRIO = p->PRIO-1;
            expiredL[p->D_PRIO].push_back(p);
          //  cout<<"expire add "<<p->pid<<endl;
        }
        else {
            activeL[p->D_PRIO].push_back(p);
        //    cout<<"active add "<<p->pid<<endl;
        }
        if (current_running != nullptr)
            //cout<<p->D_PRIO<<" "<<current_running->D_PRIO<<" "<<current_time<<" "<<des.curNextTime()<<endl;
        if (current_running != nullptr && p->D_PRIO > current_running->D_PRIO && current_time != des.curNextTime()) {
            //cout<<current_running->burst_rem<<endl;
            if (current_running->burst_temp<=quantum&&current_running->burst_rem==0){
                current_running->remaining += (current_running->burst_temp - (current_time - current_running->ST));
            }
            else current_running->remaining += (quantum - (current_time - current_running->ST));

            if (current_running->burst_temp<=quantum&&current_running->burst_rem==0){
                current_running->burst_rem += (current_running->burst_temp-(current_time - current_running->ST));
                current_running->burst_temp = current_running->burst_rem;
            }
            else current_running->burst_rem += (quantum - (current_time - current_running->ST));
            //current_running->burst_temp = current_running->burst_rem;
            //cout<<current_running->burst_rem<<endl;
            //cout<<current_running->remaining<<endl;

            //cout<<quantum<<" "<<current_running->burst_temp<<" "<<current_time - current_running->ST<<endl;
            des.removeCur();
            Event e;
            e.evtProcess = current_running;
            e.timestamp = current_time;
            e.transition = TRANS_TO_PREEMPT;
            des.put_event(e);
           // cout<<"cur running "<<current_running->pid<<endl;
            return;
        }


    }
    Process * get_next_process() {
        Process *p;
        for (int i = maxprio-1; i>=0; i--){
            if (!activeL[i].empty()){
                p = activeL[i].front();
                activeL[i].pop_front();
                //cout<<p->pid<<endl;
                return p;
            }
        }
        list<Process*>* temp;
        temp = activeL;
        activeL = expiredL;
        expiredL= temp;
        for (int i = maxprio-1; i>=0; i--){
            if (!activeL[i].empty()){
                p = activeL[i].front();
                activeL[i].pop_front();
                //cout<<p->pid<<endl;
                return p;
            }
        }
       // cout<<"null"<<endl;
        return nullptr;
    }
};


void read_rfile(char* filename){
    file.open(filename);
    string newline;
    getline(file, newline);
    while (getline(file, newline)){
        randvals.push_back(stoi(newline));
    }
    file.close();
};

int myrandom(int burst){
    if (ofs == randvals.size()){
        ofs = 0;
    }
    return 1+(randvals[ofs++] % burst);
}

void read_ifile(char* filename, int maxprio){
    string newline;
    file.open(filename);
    while(getline(file, newline)){
        stringstream sstream(newline);
        Process process;
        process.pid=processes.size();
        process.PRIO=myrandom(maxprio);
        process.D_PRIO= process.PRIO - 1;
        sstream>>process.AT;
        sstream>>process.TC;
        sstream>>process.CB;
        sstream>>process.IO;
        process.ST = -1;
        process.burst_rem = 0;
        process.burst_temp = 0;
        process.remaining = process.TC;
        process.state_ts = -1;
        process.IT = 0;
        process.CW =0;
        processes.push_back(process);

    }
    file.close();
};
Scheduler *sched;


void Simulation(){
    //cout<<"simulation start"<<endl;
    bool CALL_SCHEDULER = false;
    //int timeInPrevState;
    int burst;
    list<Process*> active[maxprio];
    list<Process*> expired[maxprio];
    for (int i=0; i<processes.size(); i++){
        Event e;
        e.timestamp = processes[i].AT;
        e.evtProcess = &processes[i];
        e.transition = TRANS_TO_READY;
        des.put_event(e);
    }
    switch (schedule){
        case F:
            sched= new FCFS();
            break;
        case L:
            sched= new LCFS();
            break;
        case S:
            sched= new SRTF();
            break;
        case R:
            sched= new RR();
            break;
        case P:
            sched= new PRIO();
            activeL = active;
            expiredL = expired;
            break;
        case E:
            sched= new PREPRIO();
            activeL = active;
            expiredL = expired;
            break;
    }
    for (Event evt = des.get_event(); evt.timestamp!=-1; evt=des.get_event()){
        Process* proc = evt.evtProcess;
        //cout<<proc->AT<<" "<<proc->TC<<" "<<proc->CB<<" "<<proc->IO<<" "<<evt.timestamp<<endl;
        current_time = evt.timestamp;
        //cout<<current_time<<endl;
        //timeInPrevState = current_time-proc->state_ts;
        int io_burst;
        switch(evt.transition){
            case TRANS_TO_READY:
              //  cout<<"Ready"<<endl;
                if (proc->state_ts==-1)
                    proc->state_ts = current_time;
                proc->state_ts = current_time;
                if (current_blocked == proc) current_blocked = nullptr;
                sched->add_to_runqueue(proc);
                CALL_SCHEDULER = true; // conditional on whether something is run
                break;
            case TRANS_TO_RUN: {
               // cout<<"Run"<<endl;
                // create event for either preemption or blocking
                proc->CW += current_time - proc->state_ts;
                proc->ST = current_time;

                current_running = proc;
                if (proc->burst_rem <= 0) {
                    //cout<<"before random burst "<<burst<<" "<<proc->burst_rem<< endl;
                    burst = myrandom(proc->CB);
                    proc->burst_rem = burst;
                    proc->burst_temp = proc->burst_rem;
                   // cout<<"random burst "<<burst<<" "<<proc->burst_rem<< endl;
                } else {
                    burst = proc->burst_rem;
                    proc->burst_temp = proc->burst_rem;
                }
                //cout<<proc->pid <<" "<<burst+proc->TC-proc->remaining <<" "<< proc->TC<<endl;
                if (proc->remaining < burst) {
                    burst = proc->remaining;
                    proc->burst_rem = burst;
                    proc->burst_temp = proc->burst_rem;
                   // cout<<"proc->remaining < burst "<<proc->remaining<<endl;
                }

                if (burst > quantum) {
                   // burst -= quantum;
                    proc->burst_rem = burst-quantum;
                    proc->burst_temp = proc->burst_rem;
                   // cout<<"burst > quantum "<<proc->burst_rem<<endl;
                    proc->remaining -= quantum;
                    Event e;
                    e.timestamp = current_time + quantum;
                    e.evtProcess = proc;
                    e.transition = TRANS_TO_PREEMPT;
                    des.put_event(e);
                } //Preempt
                else if (burst + proc->TC - proc->remaining >= proc->TC) {
                    proc->remaining -= burst;
                  //  cout<<"burst + proc->TC - proc->remaining >= proc->TC "<<proc->burst_rem<<endl;
                    Event e;
                    e.timestamp = current_time + burst;
                    e.evtProcess = proc;
                    e.transition = TRANS_TO_FINISH;
                    des.put_event(e);
                } //finish

                else if (burst + proc->TC - proc->remaining < proc->TC) {
                    proc->burst_rem = 0;
                    proc->remaining -= burst;
                  //  cout<<"burst + proc->TC - proc->remaining < proc->TC "<<proc->burst_rem<<" pid: "<<proc->pid<<endl;
                    Event e;
                    e.timestamp = current_time + burst;
                    e.evtProcess = proc;
                    e.transition = TRANS_TO_BLOCK;
                    des.put_event(e);
                } // Block
                proc->state_ts = current_time;
                break;
            }
            case TRANS_TO_BLOCK: {
              //  cout<<"Block"<<endl;
                io_burst = myrandom(proc->IO);
                int io_finish =io_burst + current_time;
                //cout<<io_burst<<endl;
                //create an event for when process becomes READY again
                if (current_blocked == nullptr) {
                    IOT += io_finish - current_time;
                    IOE = io_finish;
                    current_blocked = proc;
                }
                if (io_finish > IOE) {
                    IOT += io_finish - IOE;
                    IOE = io_finish;
                    current_blocked = proc;
                }
                //current_blocked = proc;
                current_running = nullptr;
                proc->IT += io_burst;
                proc->D_PRIO = proc->PRIO - 1;
                Event e;
                e.evtProcess = proc;
                e.timestamp = current_time + io_burst;
                e.transition = TRANS_TO_READY;
                des.put_event(e);
                CALL_SCHEDULER = true;
                proc->state_ts=current_time;
                break;
            }
            case TRANS_TO_PREEMPT:
                //cout<<"Preempt"<<endl;
                // add to runqueue (no event is generated)
                proc->state_ts = current_time;
                //cout<<current_running->pid<<endl;
                current_running->D_PRIO--;
                current_running = nullptr;
                sched->add_to_runqueue(proc);
                CALL_SCHEDULER = true;
                break;
            case TRANS_TO_FINISH:
               // cout<<"Finish"<<endl;
                proc->FT = current_time;
                proc->TT = proc->FT- proc->AT;
                current_running = nullptr;
                last_Process =current_time;
                CALL_SCHEDULER = true;
                break;
        }
        if(CALL_SCHEDULER) {
           // cout<<"run main"<<endl;
            //cout<<des.nextTimestamp()<<" "<<endl;
            if (des.nextTimestamp() == current_time){
                //cout<<"process "<<des.nextTimestamp()<<endl;
                continue;
            }
            //process next event from Event queue
            CALL_SCHEDULER = false; // reset global flag
            if (current_running == nullptr) {
                current_running = sched->get_next_process();
                //cout<<current_running->pid<<endl;
                if (current_running == nullptr){
                    continue;
                }
                Event e;
                e.timestamp = current_time;
                e.transition = TRANS_TO_RUN;
                e.evtProcess = current_running;
                des.put_event(e);
                current_running = nullptr;
            }
        }
    }
}

void printinfo(){
    int total_TC =0;
    int total_TT=0;
    int total_CW =0;
    switch (schedule){
        case F:
            cout<<"FCFS"<<endl;
            break;
        case L:
            cout<<"LCFS"<<endl;
            break;
        case S:
            cout<<"SRTF"<<endl;
            break;
        case R:
            cout<<"RR "<<quantum<<endl;
            break;
        case P:
            cout<<"PRIO "<<quantum<<endl;
            break;
        case E:
            cout<<"PREPRIO "<<quantum<<endl;
            break;
    }

    for (int i =0; i < processes.size(); i++) {
        total_TC += processes[i].TC;
        total_TT += processes[i].TT;
        total_CW += processes[i].CW;

        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
               i,
               processes[i].AT,
               processes[i].TC,
               processes[i].CB,
               processes[i].IO,
               processes[i].PRIO,
               processes[i].FT,
               processes[i].TT,
               processes[i].IT,
               processes[i].CW);
    }
    double CU = total_TC / ((double)last_Process);
    double IU = IOT / ((double)last_Process);

    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
           last_Process,
           100.0 * CU,
           100.0 * IU,
           total_TT / (double)processes.size(),
           total_CW / (double)processes.size(),
           processes.size() * 100 / (double)last_Process);
}

int main(int argc, char* argv[]) {
    int opt;
    char c;
    while ((opt = getopt(argc, argv, "vs:"))!=-1){
        switch(opt){
            case 's':
                c = optarg[0];
                if (c == 'F'){
                    schedule = F;
                }
                else if (c == 'L'){
                    schedule = L;
                }
                else if (c == 'S'){
                    schedule = S;
                }
                else if (c == 'R'){
                    schedule = R;
                    sscanf(optarg, "%*c%d", &quantum);
                    //printf("quantum= %d\n", quantum);
                }
                else if (c == 'P'){
                    schedule = P;
                    sscanf(optarg, "%*c%d:%d", &quantum, &maxprio);
                    //printf("quantum= %d\n", quantum);
                    //printf("maxprio= %d\n", maxprio);
                }
                else if (c == 'E'){
                    schedule = E;
                    sscanf(optarg, "%*c%d:%d", &quantum, &maxprio);
                    //printf("quantum= %d\n", quantum);
                    //printf("maxprio= %d\n", maxprio);
                }
                break;
        }
    }
    read_rfile(argv[optind+1]);
    read_ifile(argv[optind], maxprio);
    Simulation();
    printinfo();
    return 0;
}
