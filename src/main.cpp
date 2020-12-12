#include <iostream>
#include <ctime>
#include <random>
#include "coloring_classifier.h"
#include "shift_coloring_classifier.h"
#include "coded_bloom_filter.h"
#include "multi_bloom_filter.h"
#include "shifting_bloom_filter.h"

#define MAXN 10000

int total_collision_times = 0;
int total_failure_times = 0;
int total_error_cnt = 0;
int collision_iteration = 0;


void test_two_set()
{
    // generate 10k data randomly
    vector<pair<uint64_t, uint32_t>> data(MAXN);

    std::random_device rd;
    std::mt19937 gen(rd());
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
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    total_failure_times += !build_result;
    total_error_cnt += err_cnt;
    total_collision_times += cc->collision_time;
    if(cc->collision_time != 0) collision_iteration ++;

    delete cc;
    cc = NULL;
    data.clear();
    filter.clear();
}

int main()
{
    int N = 100;
    for(int i = 0; i < N; i++){
        cout << "Iteration " << i <<endl;
        test_two_set();
    }
    cout << "\n\n";
    cout << "Average error count is " << total_error_cnt/N <<endl;
    cout << "Total iteration with edge collision is " << collision_iteration << endl;
    cout << "Average collison times is " << total_collision_times / N << endl;
    cout << "Failure time is " << total_failure_times <<" out of "<< N <<"iterations." << endl;
    return 0;
}