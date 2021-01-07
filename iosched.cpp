#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <stdlib.h>
#include <deque>
#include <limits.h>
#include <algorithm>

using namespace std;

struct IOR{
    int id = -1;
    int AT = -1;
    int ST = -1;
    int ET = -1;
    int track = -1;
};

char alg;
ifstream file;
int num_ior;
int remain_ior;
int current_track = 0;
int direction = 1;
IOR* current_io = nullptr;
deque<IOR> io_requests;
deque<IOR> io_queue[2];
int active_queue = 0;
int add_queue = 1;
//variables to be printed
int total_time = 0;
int tot_movement = 0;
double avg_turnaround = 0;
double avg_waittime = 0;
int max_waittime = -1;

class Scheduler{
public:
    IOR* temp;
    IOR ior;
    virtual IOR* get_next_request() = 0;
};
Scheduler* sched;

class FIFO: public Scheduler{
public:
    IOR* get_next_request(){
        ior = io_queue[active_queue].front();
	temp = & ior;
        io_queue[active_queue].pop_front();
        return temp;
    }
};

class SSTF: public Scheduler{
    IOR* get_next_request(){
        int min = INT_MAX;
        int index;
        //cout<<"size: "<<io_queue.size();
        for (int i = 0; i < io_queue[active_queue].size(); i++){
            if (abs(io_queue[active_queue][i].track - current_track) < min){
                min = abs(io_queue[active_queue][i].track - current_track);
                ior = io_queue[active_queue][i];
                index = i;
            }
        }
        temp = &ior;
        io_queue[active_queue].erase(io_queue[active_queue].begin() + index);
        return temp;
    }
};

class LOOK: public Scheduler{
    IOR* get_next_request(){
        ior.id = -1;
        int min = INT_MAX;
        int index;
        for (int i = 0; i < io_queue[active_queue].size(); i++){
            if ((io_queue[active_queue][i].track - current_track) * direction >= 0 && abs(io_queue[active_queue][i].track - current_track) < min){
                min = abs(io_queue[active_queue][i].track - current_track);
                ior = io_queue[active_queue][i];
                index = i;
            }
        }
        if (ior.id == -1) {
            direction *= -1;
            //cout<<"current null"<<endl;
            for (int i = 0; i < io_queue[active_queue].size(); i++) {
                if ((io_queue[active_queue][i].track - current_track) * direction >= 0 && abs(io_queue[active_queue][i].track - current_track) < min) {
                    min = abs(io_queue[active_queue][i].track - current_track);
                    ior = io_queue[active_queue][i];
                    index = i;
                }
            }
        }
            //cout<<"+"<<endl;
            //cout<<"size: "<<io_queue.size()<<endl;
//            if (ior.id == -1){
//                //cout<<"current null"<<endl;
//                direction = -1;
//                for (int i = 0; i < io_queue.size(); i++){
//                    if (current_track > io_queue[i].track && abs(io_queue[i].track-current_track) < min){
//                        min = abs(io_queue[i].track-current_track);
//                        ior = io_queue[i];
//                        index = i;
//                    }
//                }
//            }
            //cout<<"-"<<endl;
            //cout<<"size: "<<io_queue.size()<<endl;
//        if (io_queue.size() == 1)
//        {
//            temp = &io_queue.front();
//            if (current_io->track < current_track)
//            {
//                direction = -1;
//            }
//        }
//        cout<<"size: "<<io_queue.size()<<endl;
//
//        if (ior.id == -1){
//            cout<<"current null"<<endl;
//
//        }
//      cout<<temp->id<<endl;
        temp = &ior;
        io_queue[active_queue].erase(io_queue[active_queue].begin() + index);
        return temp;
    }
};

class FLOOK: public Scheduler{
    IOR* get_next_request(){
        ior.id = -1;
        int min = INT_MAX;
        int index;
        if (io_queue[active_queue].empty()){
            //cout<<"switch"<<endl;
            active_queue = add_queue;
            add_queue = !active_queue;
        }
        for (int i = 0; i < io_queue[active_queue].size(); i++){
            if ((io_queue[active_queue][i].track - current_track) * direction >= 0 && abs(io_queue[active_queue][i].track - current_track) < min){
                min = abs(io_queue[active_queue][i].track - current_track);
                ior = io_queue[active_queue][i];
                index = i;
            }
        }
        if (ior.id == -1) {
            direction *= -1;
            for (int i = 0; i < io_queue[active_queue].size(); i++) {
                if ((io_queue[active_queue][i].track - current_track) * direction >= 0 && abs(io_queue[active_queue][i].track - current_track) < min) {
                    min = abs(io_queue[active_queue][i].track - current_track);
                    ior = io_queue[active_queue][i];
                    index = i;
                }
            }
        }
        temp = &ior;
        io_queue[active_queue].erase(io_queue[active_queue].begin() + index);
        return temp;
    }
};

class CLOOK: public Scheduler{
    IOR* get_next_request(){
        int left = INT_MIN;
        int right = INT_MAX;
        int index;
        //cout<<"direction: "<<direction<<endl;
        //cout<<"size: "<<io_queue[active_queue].size()<<endl;
        for (int i = 0; i < io_queue[active_queue].size(); i++) {
            //cout<<io_queue[active_queue][i].id<<endl;
            if (io_queue[active_queue][i].track >= current_track){
                if (io_queue[active_queue][i].track-current_track < right){
                    right = io_queue[active_queue][i].track-current_track;
                    index = i;
                    ior = io_queue[active_queue][i];
                    if (direction == -1) direction = 1;
                }
            }
        }
        if (right == INT_MAX ){
            for (int i = 0; i < io_queue[active_queue].size(); i++) {
                if (io_queue[active_queue][i].track < current_track){
                    if (current_track - io_queue[active_queue][i].track > left){
                        left = current_track - io_queue[active_queue][i].track;
                        index = i;
                        ior = io_queue[active_queue][i];
                        if (direction == 1) direction = -1;
                    }
                }
            }
        }
        //cout<<"direction: "<<direction<<endl;
        temp = &ior;
        io_queue[active_queue].erase(io_queue[active_queue].begin() + index);
        return temp;
    }
};

void read_infile(char* filename) {
    string newline;
    file.open(filename);
    int index = 0;
    while(getline(file, newline)) {
        if (newline[0] == '#') {
            continue;
        }
        stringstream sstream(newline);
        IOR ior;
        sstream>>ior.AT;
        sstream>>ior.track;
        ior.id = index;
        num_ior = index;
        index++;
        io_requests.push_back(ior);
    }
    //total_time = io_requests[0].AT;
    file.close();

}

void simulation(){
    remain_ior = num_ior + 1;
    while(remain_ior){
        //cout<<"current time: "<<total_time<<" current track"<<current_track<<endl;
        //If a new I/O arrived to the system at this current time → add request to IO-queue
        if (total_time == io_requests.front().AT){
            //cout<<"step 1"<<endl;
            if (alg == 'f') io_queue[add_queue].push_back(io_requests.front());
            else io_queue[active_queue].push_back(io_requests.front());
            io_requests.pop_front();
        }

        //If an IO is active and completed at this time → Compute relevant info and store in IO request for final summary
        if (current_io != nullptr && current_track == current_io->track){
            //cout<<"step 2"<<endl;
            current_io->ET = total_time;
            io_requests.push_back(*current_io);
            remain_ior--;
            current_io = nullptr;
        }

        //If an IO is active but did not yet complete → Move the head by one sector/track/unit in the direction it is going (to simulate seek)
        if (current_io != nullptr && current_track != current_io->track){
            //cout<<"step 3"<<endl;
            current_track += direction;
            tot_movement++;
        }

        //If no IO request active now (after (2)) but IO requests are pending → Fetch the next request and start the new IO.
        if (current_io == nullptr && (!io_queue[active_queue].empty() || !io_queue[add_queue].empty())){
            //cout<<"step 4"<<endl;
            current_io = sched->get_next_request();
            //cout<<"current_io: "<<current_io->id<<endl;
            current_io->ST = total_time;
            if (current_track == current_io->track) continue;
            if (current_track < current_io->track) direction = 1;
            else direction = -1;
            current_track += direction;
            tot_movement++;
        }

        //If no IO request is active now and no IO requests pending→exit simulation.
//        if (current_io == nullptr && io_queue.empty()){
//            cout<<"step final"<<endl;
//            break;
//        }
        //Increment time by 1
        total_time++;
        //if (total_time == 250) break;
    }
}
bool compare(IOR a, IOR b){
    if (a.id < b.id) return true;
    else return false;
}
void printinfo(){
    total_time--;
    sort(io_requests.begin(),io_requests.end(), compare);
    for (int i = 0; i < io_requests.size(); i++) {
        if (max_waittime < io_requests[i].ST - io_requests[i].AT) max_waittime = io_requests[i].ST - io_requests[i].AT;
        avg_waittime += io_requests[i].ST - io_requests[i].AT;
        avg_turnaround += io_requests[i].ET - io_requests[i].AT;
        printf("%5d: %5d %5d %5d\n",i,io_requests[i].AT,io_requests[i].ST,io_requests[i].ET);
    }
    //cout<<tot_waittime<<" "<<tot_turnaround<<endl;
    avg_turnaround /= (double)io_requests.size();
    avg_waittime /= (double)io_requests.size();
    printf("SUM: %d %d %.2lf %.2lf %d\n", total_time, tot_movement, avg_turnaround, avg_waittime, max_waittime);
}

int main(int argc, char* argv[]) {
    getopt(argc, argv, "s:");
    alg = optarg[0];
    read_infile(argv[optind]);
    switch (alg){
        case 'i':
            sched = new FIFO();
            break;
        case 'j':
            sched = new SSTF();
            break;
        case 's':
            sched = new LOOK();
            break;
        case 'c':
            sched = new CLOOK();
            break;
        case 'f':
            sched = new FLOOK();
            break;
    }
    simulation();
    printinfo();

//    int a =INT_MAX;
//    if (a == INT_MAX) cout<<"yes"<<endl;
//
//    deque<int> test;
//    test.push_back(0);
//    test.push_back(1);
//    test.push_back(2);
//    test.push_back(3);
//    int* temp = &test[2];
//    cout<<*temp<<endl;
//    test.erase(test.begin()+2);
//    cout<<*temp<<endl;

    return 0;
}

