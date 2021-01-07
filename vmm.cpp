#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <vector>
#include <deque>

using namespace std;

const int num_PTE = 64;
int num_frames;
int ins_count = 0;
int exit_count = 0;
unsigned long c_total = 0;
unsigned long cost = 0;
char alg;

struct Frame{
    int fid;
    int pid = -1;
    int vpage = -1;
    unsigned int age = 0;
    int time = 0;
};
deque<Frame> frame_t;
deque<Frame*> free_frame_t;

struct VMA{
    unsigned int starting_virtual_page : 7;
    unsigned int ending_virtual_page : 7;
    unsigned int write_protected : 1;
    unsigned int file_mapped : 1;
};

struct PTE{
    unsigned int present : 1 = 0;
    unsigned int write_protect : 1 = 0;
    unsigned int modified : 1 = 0;
    unsigned int referenced : 1 = 0;
    unsigned int pagedout : 1 = 0;
    unsigned int filemapped : 1 = 0;
    unsigned int frame : 7 = 0;
};

struct Instruction{
    char ins;
    int num;
};
vector<Instruction> instruction_t;


struct Process{
    vector<VMA> vma_t;
    vector<PTE> pte_t;
    unsigned int pid;
    unsigned long unmaps = 0;
    unsigned long maps = 0;
    unsigned long pageins = 0;
    unsigned long pageouts = 0;
    unsigned long fins = 0;
    unsigned long fouts = 0;
    unsigned long zeros = 0;
    unsigned long segv = 0;
    unsigned long segprot = 0;
};
vector<Process> processes;
Process* cur_process;
vector<int> randvals;
ifstream file;
int ofs=0;

int myrandom(int burst){
    if (ofs == randvals.size()){
        ofs = 0;
    }
    return randvals[ofs++] % burst;
}
void read_rfile(char* filename){
    //cout<<"start rfile "<<filename<<endl;
    file.open(filename);
    string newline;
    getline(file, newline);
    while (getline(file, newline)){
        randvals.push_back(stoi(newline));
    }
    file.close();
}

class Pager{
public:
    virtual Frame* select_victim_frame() = 0;
};

class FIFO : public Pager{
    int i = 0;
public:
    Frame* select_victim_frame(){
        if (i == num_frames) i = 0;
        return &frame_t[i++];
    }
};

class Random : public Pager{
public:
    Frame* select_victim_frame() {
        return &frame_t[myrandom(num_frames)];
    }
};

class Clock : public Pager{
    int i = 0;
    Frame* hand = &frame_t.front();
    Frame* temp;
public:
    Frame* select_victim_frame() {
        while (processes[hand->pid].pte_t[hand->vpage].referenced){
            i++;
            if (i == num_frames) i = 0;
            processes[hand->pid].pte_t[hand->vpage].referenced = 0;
            hand = &frame_t[i];
        }
        temp = hand;
        i++;
        if (i == num_frames) i = 0;
        hand = &frame_t[i];
        return temp;
    }
};

class NRU : public Pager{
    Frame* hand = &frame_t.front();
    int index = 0;
    int repeat = 0;
    bool add = false;
public:
    Frame* select_victim_frame() {
      //  cout<<"enter"<<endl;
        int c0 = 0;
        int c1 = 0;
        int c2 = 0;
        int c3 = 0;
        Frame* nru_t[4][num_frames];
        for (int i = 0; i < num_frames; i++){
            hand = &frame_t[(i+index)%num_frames];
            if (processes[hand->pid].pte_t[hand->vpage].present){
                if (!processes[hand->pid].pte_t[hand->vpage].referenced && !processes[hand->pid].pte_t[hand->vpage].modified)
                    nru_t[0][c0++] = hand;
                else if (!processes[hand->pid].pte_t[hand->vpage].referenced && processes[hand->pid].pte_t[hand->vpage].modified)
                    nru_t[1][c1++] = hand;
                else if (processes[hand->pid].pte_t[hand->vpage].referenced && !processes[hand->pid].pte_t[hand->vpage].modified)
                    nru_t[2][c2++] = hand;
                else if (processes[hand->pid].pte_t[hand->vpage].referenced && processes[hand->pid].pte_t[hand->vpage].modified)
                    nru_t[3][c3++] = hand;
            }
            if (ins_count-repeat>=50){
                processes[hand->pid].pte_t[hand->vpage].referenced = 0;
                add = true;
            }
        }
        if (add) {
            repeat=ins_count;
            add = false;
        }
        //cout<<c0<<" "<<c1<<" "<<c2<<" "<<c3<<" "<<endl;
        for (int i = 0; i < 4; i++){
            if (c0) {
                index = (nru_t[0][0]->fid+1) % num_frames;
                return nru_t[0][0];
            }
            if (c1) {
                index = (nru_t[1][0]->fid+1) % num_frames;
                return nru_t[1][0];
            }
            if (c2) {
                index = (nru_t[2][0]->fid+1) % num_frames;
                return nru_t[2][0];
            }
            if (c3) {
                index = (nru_t[3][0]->fid+1) % num_frames;
                return nru_t[3][0];
            }
        }
    }
};

class Aging : public Pager{
    int index = 0;
    Frame* hand;
    Frame* temp ;
public:
    Frame* select_victim_frame() {
        hand = &frame_t[index];
        for (int i = 0; i < num_frames; i++){
            temp = &frame_t[(i + index) % num_frames];
            temp->age = temp->age >> 1;
            if (processes[temp->pid].pte_t[temp->vpage].referenced){
                processes[temp->pid].pte_t[temp->vpage].referenced = 0;
                temp->age = temp->age | 0x80000000;
            }
            if (temp->age < hand->age)
                hand = temp;
        }
        index = (hand -> fid + 1) % num_frames;
        return hand;
    }
};

class Working_Set : public Pager{
    int index = 0;
    Frame* hand;
public:
    Frame* select_victim_frame(){
        int last_age = -1;
        hand = &frame_t[index];
        Frame* temp;
        for (int i = 0; i < num_frames; i++){
            temp = &frame_t[(i + index) % num_frames];
            int age = ins_count - temp->time;
            if (processes[temp->pid].pte_t[temp->vpage].referenced){
                processes[temp->pid].pte_t[temp->vpage].referenced = 0;
                temp->time = ins_count;
            }
            else if (age >= 50){
                index = (temp-> fid + 1) % num_frames;
                return temp;
            }
            else if (age > last_age){
                last_age = age;
                hand = temp;
            }
        }
        index = (hand -> fid + 1) % num_frames;
        return hand;
    }
};

Pager* pager;
bool O = false, P = false, F = false, S = false;

Frame* get_frame(){
    Frame* frame = free_frame_t.front();
    if (!free_frame_t.empty()) {
        free_frame_t.pop_front();
        return frame;
    }
    else return pager->select_victim_frame();
}



void read_infile(char* filename){
    string newline;
    file.open(filename);
    int p_num,vma_num;
    while(getline(file, newline)){
        if (newline[0] == '#'){
            continue;
        }
        else {
            p_num = stoi(newline);
            break;
        }
    }
    //cout<<p_num<<endl;

    for (int i = 0; i < p_num; i++){
        getline(file, newline);
        getline(file, newline);
        getline(file, newline);
        vma_num = stoi(newline);
        //cout<<vma_num<<endl;
        Process process;
        process.pid = i;
        for (int j = 0; j < vma_num; j++){
            getline(file, newline);
            stringstream sstream(newline);
            VMA vma;
            int startpage, endpage, writeprotected, filemapped;
            sstream>>startpage;
            sstream>>endpage;
            sstream>>writeprotected;
            sstream>>filemapped;
//            cout<<startpage<<endl;
//            cout<<endpage<<endl;
//            cout<<writeprotected<<endl;
//            cout<<filemapped<<endl;
            vma.starting_virtual_page = startpage;
            vma.ending_virtual_page = endpage;
            vma.write_protected = writeprotected;
            vma.file_mapped = filemapped;
            process.vma_t.push_back(vma);
        }
        for (int k = 0; k < num_PTE; k++){
            PTE pte;
            pte.frame = k;
            process.pte_t.push_back(pte);
        } //initialize process PageTable
        //cout<<process.vma_t.size()<<endl;
       // cout<<process.vma_t[1].write_protected<<endl;
//        for (int m = 0; m < num_PTE; m++) {
//            cout<<process.pte_t[m].frame<<endl;
//        }

        for (int n = 0; n < process.vma_t.size(); n++){
            for (int m = 0; m < num_PTE; m++) {
                if (process.vma_t[n].starting_virtual_page <= process.pte_t[m].frame
                    && process.vma_t[n].ending_virtual_page >= process.pte_t[m].frame) {
                   // cout<<"same " <<m<<" "<<n<<process.vma_t[n].write_protected<<endl;
                    process.pte_t[m].write_protect = process.vma_t[n].write_protected;
                    process.pte_t[m].filemapped = process.vma_t[n].file_mapped;
                }
            }
        } //initialize process pageTable write_protected & file_mapped
        processes.push_back(process);
    }
    getline(file, newline);
    while(getline(file, newline)){
        if (newline[0] == '#')
            break;
        Instruction instruction;
        stringstream sstream(newline);
        sstream>>instruction.ins;
        sstream>>instruction.num;
//        cout<<instruction.ins<<endl;
//        cout<<instruction.num<<endl;
        instruction_t.push_back(instruction);
    }
    file.close();
}

void simulation(){
    while (instruction_t.size()!=0){
        Instruction cur_inst = instruction_t.front();
        instruction_t.erase(instruction_t.begin()); //get next instruction

        if (O){
            cout << ins_count << ": ==> " << cur_inst.ins << " " << cur_inst.num << endl;
        }

        switch(cur_inst.ins){ //operate instruction
            case 'c':
                cur_process = &processes[cur_inst.num];
                ins_count++;
                c_total++;
                cost += 121;
                break;
            case 'e':
                if (cur_inst.num == cur_process->pid)
                    cout << "EXIT current process " << cur_inst.num << endl;
                else cout << "something wrong when exit"<<endl;

                for (int i = 0; i < num_PTE; i++)
                {
                    if (cur_process->pte_t[i].present)
                    {
                        cout << " UNMAP " << frame_t[cur_process->pte_t[i].frame].pid
                             << ":"       << frame_t[cur_process->pte_t[i].frame].vpage << endl;
                        frame_t[cur_process->pte_t[i].frame].pid = -1;
                        frame_t[cur_process->pte_t[i].frame].vpage = -1;
                        frame_t[cur_process->pte_t[i].frame].age = 0;
                        frame_t[cur_process->pte_t[i].frame].time = 0;
                        free_frame_t.push_back(&frame_t[cur_process->pte_t[i].frame]);
                        cost += 400;
                        cur_process->unmaps++;

                        if (cur_process->pte_t[i].modified && cur_process->pte_t[i].filemapped)
                        {
                            cost += 2500;
                            processes[cur_inst.num].fouts++;
                            cout << " FOUT" << endl;
                        }
                    }
                    cur_process->pte_t[i].present = 0;
                    cur_process->pte_t[i].referenced = 0;
                    cur_process->pte_t[i].pagedout = 0;
                    cur_process->pte_t[i].frame = 0;
                    cur_process->pte_t[i].modified = 0;
                    cur_process->pte_t[i].write_protect = 0;
                }
                exit_count++;
                ins_count++;
                cost += 175;
                break;
            default:  //read or write
                cost++;
                ins_count++;
                bool valid = false;
                if (!cur_process->pte_t[cur_inst.num].present){
                    for (int i = 0; i < cur_process->vma_t.size(); i++){
                        if (cur_process->vma_t[i].starting_virtual_page <= cur_inst.num
                            && cur_process->vma_t[i].ending_virtual_page >= cur_inst.num){
                            valid = true;
                            break;
                        }
                    }
                    if (!valid){
                        cost += 240;
                        cur_process->segv++;
                        if (O) cout<< " SEGV"<<endl;
                        continue;
                    }
                    Frame* next_frame = get_frame();
                    //used frame, reset it
                    if (next_frame->pid != -1 & next_frame->vpage != -1){
                        PTE* pte = &processes[next_frame->pid].pte_t[next_frame->vpage];
                        processes[next_frame->pid].unmaps++;
                        cost += 400;

                        if (O) cout<<" UNMAP "<<next_frame->pid<<":"<<next_frame->vpage<<endl;
                        if (pte->modified){
                            if (pte->filemapped){
                                processes[next_frame->pid].fouts++;
                                cost += 2500;
                                if (O) cout<<" FOUT"<<endl;
                            }
                            else{
                                pte->pagedout = 1;
                                processes[next_frame->pid].pageouts++;
                                cost += 3000;
                                if (O) cout<<" OUT"<<endl;
                            }
                        }
                        pte->present = 0;
                        pte->frame = 0;
                        pte->modified = 0;
                        pte->referenced = 0;

                    }

                    if (cur_process->pte_t[cur_inst.num].filemapped){
                        cur_process->fins++;
                        cost += 2500;
                        if (O) cout<<" FIN"<<endl;
                    }
                    else if (!cur_process->pte_t[cur_inst.num].filemapped & !cur_process->pte_t[cur_inst.num].pagedout){
                        cur_process->zeros++;
                        cost += 150;
                        if (O) cout<<" ZERO"<<endl;
                    }
                    else if(!cur_process->pte_t[cur_inst.num].filemapped & cur_process->pte_t[cur_inst.num].pagedout){
                        cur_process->pageins++;
                        cost += 3000;
                        if (O) cout<<" IN"<<endl;
                    }

                    //unused frame
                    cur_process->pte_t[cur_inst.num].frame = next_frame->fid;
                    cur_process->pte_t[cur_inst.num].present = 1;
                    cur_process->maps++;
                    cost += 400;
                    if (O) cout << " MAP " << next_frame->fid << endl;
                    next_frame->pid = cur_process->pid;
                    next_frame->vpage = cur_inst.num;
                    next_frame->age = 0;
                    next_frame->time = ins_count;
                }
                cur_process->pte_t[cur_inst.num].referenced = 1;
                if (cur_inst.ins == 'w'){
                    if (cur_process->pte_t[cur_inst.num].write_protect){
                        cur_process->segprot++;
                        cost += 300;
                        if (O) cout << " SEGPROT" << endl;
                    }
                    else cur_process->pte_t[cur_inst.num].modified = 1;
                }
        }
    }
}

void printInfo(){
    if (P){
        for (int j = 0; j < processes.size(); j++) {
            Process x = processes[j];
            cout << "PT[" << j << "]: ";
            for (int i = 0; i < num_PTE; i++){
                if (x.pte_t[i].present){
                    cout << i << ":";
                    if (x.pte_t[i].referenced)
                        cout << "R";
                    else
                        cout << "-";
                    if (x.pte_t[i].modified)
                        cout << "M";
                    else
                        cout << "-";
                    if (x.pte_t[i].pagedout)
                        cout << "S ";
                    else
                        cout << "- ";
                }
                else{
                    if (x.pte_t[i].pagedout)
                        cout << "# ";
                    else
                        cout << "* ";
                }
            }
            cout << endl;
        }
    }
    if (F){
        cout << "FT: ";
        for (int i = 0; i < frame_t.size(); i++){
            if (frame_t[i].pid == -1)
                cout << "* ";
            else cout << frame_t[i].pid << ":" << frame_t[i].vpage << " ";
        }
        cout << endl;
    }
    if (S){
        for (int i = 0; i < processes.size(); i++){
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                   processes[i].pid,processes[i].unmaps,processes[i].maps,processes[i].pageins,
                   processes[i].pageouts, processes[i].fins, processes[i].fouts,
                   processes[i].zeros, processes[i].segv, processes[i].segprot);
        }
        printf("TOTALCOST %lu %lu %lu %lu\n", ins_count, c_total, exit_count, cost);
    }
}

int main(int argc, char* argv[]){
    int opt;
    while((opt = getopt(argc, argv, "f:a:o:")) != -1){
        switch(opt){
            case 'f':
                num_frames = atoi(optarg);
                //cout<<"num_frames "<<num_frames<<endl;
                for (int i = 0; i < num_frames; i++){
                    Frame frame;
                    frame.fid = i;
                    frame_t.push_back(frame);
                    free_frame_t.push_back(&frame_t[i]);
                }
                break;
            case 'a':{
                alg = optarg[0];
                //cout<<"a: "<<optarg[0]<<endl;
                break;
            }
            case 'o':
                for (int i = 0; i < strlen(optarg); i++){
                    switch (optarg[i]){
                        case 'O':
                            O = true;
                            break;
                        case 'P':
                            P = true;
                            break;
                        case 'F':
                            F = true;
                            break;
                        case 'S':
                            S = true;
                            break;
                    }
                    //cout<<optarg[i]<<endl;
                }
                break;
        }
    }

    read_infile(argv[optind]);
    read_rfile(argv[optind+1]);

    switch (alg){
        case 'f':
            pager = new FIFO();
            break;
        case 'r':
            pager = new Random();
            break;
        case 'c':
            pager = new Clock();
            break;
        case 'e':
            pager = new NRU();
            break;
        case 'a':
            pager = new Aging();
            break;
        case 'w':
            pager = new Working_Set();
            break;
    }
    simulation();
    printInfo();
    return 0;
}
