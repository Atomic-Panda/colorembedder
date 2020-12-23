#include <iostream>
#include <ctime>
#include <random>
#include "coloring_classifier.h"
#include "shift_coloring_classifier.h"
#include "coded_bloom_filter.h"
#include "multi_bloom_filter.h"
#include "shifting_bloom_filter.h"
#include<chrono>

#define MAXN 10000
#define insertN 1000

int total_collision_times = 0;
int total_failure_times = 0;
int total_error_cnt = 0;
int collision_iteration = 0;
vector<int> oft_size;
int err_0_cnt = 0;

void test_two_set()
{
    // generate 10k data randomly
    vector<pair<uint64_t, uint32_t>> data(MAXN + insertN);

    // std::random_device rd;
    // std::mt19937 gen(rd());
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    mt19937 gen(seed);	 // 大随机数
    std::uniform_int_distribution<uint64_t> dis(0, 1ull << 40);

    unordered_set<uint64_t> filter;

    for (int i = 0; i < MAXN; ++i) {
        uint64_t item;
        while (1) {
            item = dis(gen);
            if (filter.find(item) == filter.end()) {
                filter.insert(item);
                break;
            }
        }
        data[i].first = item;
        data[i].second = (i % 2 == 0);
    }

    auto cc = new ShiftingColoringClassifier<int(MAXN * 1.11), 4, 2>();

    bool build_result = cc->build(data, MAXN);
    cc -> report();
    cout << "Build 4-color classifier for " << MAXN << " items" << endl;
    cout << "\tUsing " << int(MAXN * 1.11) << " buckets" << endl;
    cout << "\tBuild result: " << (build_result ? "success": "failed") << endl;

    int err_cnt = 0;
    if (build_result) {
        for (int i = 0; i < MAXN; ++i) {
            uint32_t result = cc->query(data[i].first);
            err_cnt += (result != data[i].second);
        }
        cout << "\tError count: " << err_cnt << endl;
    }
    else{
        oft_size.push_back(0);
        return;
    }
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    // total_failure_times += !build_result;
    // total_error_cnt += err_cnt;
    // total_collision_times += cc->collision_time;
    // if(cc->collision_time != 0) collision_iteration ++;

    // test for insert
    for(int i = MAXN; i < MAXN + insertN; i++){
        // cout <<"Insert " << i - MAXN + 1 << endl;
        uint64_t item;
        while (1) {
            item = dis(gen);
            if (filter.find(item) == filter.end()) {
                filter.insert(item);
                break;
            }
        }
        data[i].first = item;
        data[i].second = (i % 2 == 0);
        bool flag = cc -> insert(data[i].first, data[i].second);
        if(!flag){
            // cout << "Insert Failed" <<endl;
            int tmp = 0;
            for (int j = 0; j <= i; ++j) {
                uint32_t result = cc->query(data[j].first);
                tmp += (result != data[j].second);
                // if(result != data[j].second){
                //     cout << j << " ";
                // }
                
            }
            if (tmp != 0){
                cc->report();
                cout << "Error count: " << tmp << endl <<endl;
                err_0_cnt ++;
                break;
            }
        }
    }
    // err_cnt = 0;
    // for (int i = 0; i < MAXN + insertN; ++i) {
    //     uint32_t result = cc->query(data[i].first);
    //     err_cnt += (result != data[i].second);
    //     if(result != data[i].second){
    //         cout << i << " ";
    //     }
    // }
    cout << endl;
    cout << "After insert " << insertN << " elements..." << endl;
    cc->report();
    cout << "Error count: " << err_cnt << endl;
    cout << endl;
    oft_size.push_back(cc->OverFlowTable.size());
    delete cc;
    cc = NULL;
    data.clear();
    filter.clear();
}

int main()
{
    int N = 30;
    for(int i = 0; i < N; i++){
        cout << "Iteration " <<i <<endl;
        test_two_set();
    }
    int max_size = 0;
    for(int i = 0; i < N; i++){
        cout << oft_size[i]<<" ";
        if(oft_size[i] > max_size){
            max_size = oft_size[i];
        }
    }
    cout << endl;
    cout << max_size<<endl;
    cout <<"error_0_cnt: " << err_0_cnt <<endl;
    return 0;
}

// int N = 1;
// for(int i = 0; i < N; i++){
//     cout << "Iteration " << i <<endl;
//     test_two_set();
// }
// cout << "\n\n";
// cout << "Average error count is " << total_error_cnt/N <<endl;
// cout << "Total iteration with edge collision is " << collision_iteration << endl;
// cout << "Average collison times is " << total_collision_times / N << endl;
// cout << "Failure time is " << total_failure_times <<" out of "<< N <<" iterations." << endl;
/*
 *Average error count is 0
 *Total iteration with edge collision is 970
 *Average collison times is 31
 *Failure time is 70 out of 1000 iterations. 
 */