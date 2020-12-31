#ifndef COLORINGCLASSIFER_COLORING_CLASSIFIER_H
#define COLORINGCLASSIFER_COLORING_CLASSIFIER_H


#define MAX_LEN 160

#include <cstdio>
#include <cstdint>
#include <vector>
#include <cstring>
#include <unordered_map>
#include "BOB_hash.h"
#include <unordered_set>
#include <list>
#include <queue>
#include <ctime>

#define MAX_EDGE_COLLISION_TIME 1

using namespace std;

BOBHash * hash1, * hash2;

template<int32_t bucket_num, int32_t COLOR_NUM = 4, bool verbose = 0>
class ColoringClassifier
{
    // buckets是unit8，如果是4个颜色的话，2 bit就行了
    // Transferring v_bucket and bucket in the 2 function sync, and we actually don't use bucket in the algorithm
    uint8_t buckets[bucket_num];
private:
    template<uint32_t hash_range>
    struct Edge
    {
        // 被hash的可以是str，也可以是int64
        uint64_t e;
        uint32_t hash_val_a;
        uint32_t hash_val_b;
        bool available;
        char e_str[MAX_LEN];

        // 设置Hash的值，val_a and val_b，即两个bucket
        void set_hash_val(uint64_t _e) {
            e = _e;
            // 这边的4是指size，可是为什么是4呢？可能需要研究一下BOB_Hash
            hash_val_a = hash1->run(&e, 4) % hash_range;
            hash_val_b = hash2->run(&e, 4) % hash_range;
            int i = 1;
            while (hash_val_a == hash_val_b) {
                hash_val_b = (hash1->run(&e, 4) +
                    (i++) * hash2->run(&e, 4)) % hash_range;
            }
        }

        void set_hash_val(const char * str) {
            strcpy(e_str, str);
            hash_val_a = hash1->run(e_str, strlen(e_str)) % hash_range;
            hash_val_b = hash2->run(e_str, strlen(e_str)) % hash_range;
            int i = 1;
            while (hash_val_a == hash_val_b) {
                hash_val_b = (hash1->run(&e, 4) +
                    (i++) * hash2->run(&e, 4)) % hash_range;
            }
        }

        // 5个构造函数，前3个直接构造，后面2个复制构造
        Edge() : available(true) {}
        Edge(uint64_t _e) : available(true) {
            set_hash_val(_e);
        }

        Edge(const char * e_str) : available(true) {
            set_hash_val(e_str);
        }

        Edge(const Edge & edge, int offset) {
            available = edge.available;
            e = edge.e;
            strcpy(e_str, edge.e_str);
            hash_val_a = (edge.hash_val_a + offset) % hash_range;
            hash_val_b = (edge.hash_val_b + offset) % hash_range;
        }
        Edge(const Edge& edge) {
            available = edge.available;
            e = edge.e;
            strcpy(e_str, edge.e_str);
            hash_val_a = edge.hash_val_a;
            hash_val_b = edge.hash_val_b;
        }

        // query the other node of this edge
        uint32_t get_other_val(uint32_t i) const {
            if (i == hash_val_a) {
                return hash_val_b;
            }
            if (i == hash_val_b) {
                return hash_val_a;
            }
            exit(-1);
        }
    };


public:
    int edge_collision_num;
    int affected_node_num;
    int node_num_to_update;
    int BUCKET_NUM = bucket_num;
    int collision_time;
    string name;

protected:
    typedef Edge<bucket_num> CCEdge;
    // 正边和负边分开记录，构造时会用到
    vector<CCEdge *> pos_edges, neg_edges;
public:
    struct overflowtable{
        unordered_map<uint64_t, uint32_t> ErrorTable;

        int size(){
            return ErrorTable.size();
        }

        int query(uint64_t e){
            if(ErrorTable.find(e)==ErrorTable.end()){
                return -1;
            }
            return ErrorTable[e];
        }
        
        void insert(uint64_t e, uint32_t classid){
            ErrorTable.insert(make_pair(e, classid));
        }
    }OverFlowTable;

private:
    struct VerboseBuckets;

    // group(nodes with same color) struct
    struct VerboseGroup{
        int color;
        unordered_set<VerboseGroup *> neighbours;
        // unordered_set<VerboseGroup *> removed_neighbours;
        bool deleted;
        bool visited;
        bool used;
        VerboseBuckets * back_pointer;
        int deleted_neighbour_num;
        int remained_neighbour_num;
        VerboseGroup() : color(-1), deleted(false), visited(false), used(false),
                         back_pointer(NULL), deleted_neighbour_num(0), remained_neighbour_num(0) {}
    };

    // 和buckets一一对应，verbose的冗长版本；node struct, link to pos, neg edges, root_bucket, next_bucket and last_son
    struct VerboseBuckets{
        int bucket_id;
        int color;
        vector<CCEdge *> pos_edges, neg_edges;
        VerboseBuckets * root_bucket;
        VerboseBuckets * next_bucket;
        VerboseBuckets * last_son;

        VerboseGroup group;

        VerboseBuckets(): color(-1) {
            // root_bucket是this
            root_bucket = this;
            // next_bucket设置为null
            next_bucket = NULL;
            last_son = this; // only useful when root
            pos_edges.clear();
            neg_edges.clear();

            group.back_pointer = this;
            bucket_id = -1;
        }

        // GetRoot？并查集？
        VerboseBuckets * get_root_bucket()
        {
            VerboseBuckets * ret = root_bucket;
            while (ret != ret->root_bucket) {
                ret = ret->root_bucket;
            }

            if (ret != this) {
                root_bucket = ret;
            }

            return ret;
        }

        void set_root_bucket(VerboseBuckets * new_root)
        {
            VerboseBuckets * old_root = get_root_bucket();
            old_root->root_bucket = new_root;
            if(new_root->last_son->next_bucket != NULL) cout <<"!!!!!!!!!!!!!!!!!!!!!!"<<endl;
            new_root->last_son->next_bucket = old_root;
            new_root->last_son = old_root->last_son;
        }
    } v_buckets[bucket_num];
    // duplication of v_buckets used to insert
    VerboseBuckets old_buckets[bucket_num];
    // v_buckets就是node节点，这里有bucket_num个

    // 这两个sync函数是为了实现2bit的空间占用，把四个v_bucket放到一个uint8_t的bucket里面
    void synchronize_all(){
        memset(buckets, 0, sizeof(buckets));
        for (int i = 0; i < bucket_num; ++i) {
            if (COLOR_NUM == 3) {
                int bucket_id = i / 5;
                const int val_table[] = {
                        1, 3, 9, 27, 81,
                };
                buckets[bucket_id] += v_buckets[i].color * val_table[i % 5];
            } else if (COLOR_NUM == 4) {
                int bucket_id = i / 4;
                buckets[bucket_id] |= ((v_buckets[i].color & 0x3) << ((i % 4) * 2));
            } else {
                buckets[i] = uint8_t(v_buckets[i].color);
            }
        }
    }

    void synchronize(int i){
        if (COLOR_NUM == 3) {
            int bucket_id = i / 5;
            int pos = i % 5;
            const int val_table[] = {
                    1, 3, 9, 27, 81,
            };
            int old_val = (buckets[bucket_id] / val_table[pos]) % 3;
            buckets[bucket_id] += (v_buckets[i].color - old_val) * val_table[i % 5];
        } else if (COLOR_NUM == 4) {
            int bucket_id = i / 4;
            buckets[bucket_id] &= ~((0x3) << ((i % 4) * 2));
            buckets[bucket_id] |= ((v_buckets[i].color & 0x3) << ((i % 4) * 2));
        } else {
            buckets[i] = uint8_t(v_buckets[i].color);
        }
    }

protected:
    inline int get_bucket_val(int idx)
    {
        // return v_buckets directly
        return v_buckets[idx].color;
        /* 
        if (COLOR_NUM == 3) {
            int bucket_id = idx / 5;
            const int val_table[] = {
                    1, 3, 9, 27, 81
            };
            return (buckets[bucket_id] / val_table[idx % 5]) % 3;
        } else if (COLOR_NUM == 4) {
            int bucket_id = idx / 4;
            return (buckets[bucket_id] >> ((idx % 4) * 2)) & 0x3;
        } else {
            return buckets[idx];
        }*/
    }
public:
    struct updatecc{
        int tot_num;
        vector<int> affected_id;
        int st, ed;
        updatecc(){
            affected_id.clear();
            tot_num = 0;
            st = ed = -1;
        }
        void insert(int i){
            tot_num ++;
            affected_id.push_back(i);
            if(i < st || st == -1){
                st = i;
            }
            if(i > ed || st == -1){
                ed = i;
            }
        }
        void dis(){
            cout << "Updated: " << tot_num <<" ";
            cout << st <<" " << ed <<": ";
            for(auto it = affected_id.begin(); it != affected_id.end(); it++){
                cout << *it <<" ";
            }
            cout << endl;
        }
        void update(ColoringClassifier* cc){
            for(auto it = affected_id.begin(); it != affected_id.end(); it++){
                cc->synchronize(*it);
            }
        }
        void clear(){
            tot_num = 0;
            st = ed = -1;
            affected_id.clear();
        }
    }UpdateCC;
private:
    static int get_next_color(VerboseGroup * g)
    {
        int try_color = g->color;
        int init_color = int(((uint64_t(g)) >> 7) % COLOR_NUM);
        if (try_color == -1) {
            try_color = init_color;
        }

        while (1) {
            try_color = (try_color + 1) % COLOR_NUM;
            if (try_color == init_color) {
                return -1;
            }

            bool flag = true;
            for (auto n: g->neighbours) {
                if (n->color == try_color) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                return try_color;
            }
        }
    };

    // try color groups using brute force method (enum)
    // if failed, we need to restore the color
    static bool try_color_groups_bf(vector<VerboseGroup *> & groups)
    {
        unsigned int now = 0;
        int old_color[bucket_num] = {};
        for(long long unsigned int i = 0; i < groups.size(); i++){
            old_color[i] = groups[i]->color;
        }

        while (now >= 0 && now < groups.size()) {
            VerboseGroup * g = groups[now];
            int color = get_next_color(g);
            if (color == -1) {
                g->color = -1;
                now--;
                continue;
            }
            g->color = color;
            now++;
        }

        bool result = (now == groups.size());

        if (result) {
            for (auto g: groups) {
                color_group(g->back_pointer, g->color);
            }
        }
        // if failed, restore the color to the old one
        else{
            for(long long unsigned int i = 0; i < groups.size(); i++){
                groups[i]->color = old_color[i];
            }
        }

        return result;
    };

    // try color groups
    // if success, color them and return true
    bool try_color_groups(vector<VerboseGroup *> & groups)
    {
        vector<VerboseGroup *> sorted(groups.size());
        int tot_deleted = 0;

        if (verbose) {
            printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
            "start coloring group...\n"
            "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
        }

        // try a more aggressive way to color
        VerboseGroup ** rotate_queue = new VerboseGroup*[2 * groups.size() + 1];
        for (int i = 0; i < (int)groups.size(); ++i) {
            rotate_queue[i] = groups[i];
            rotate_queue[i]->visited = false;
            rotate_queue[i]->remained_neighbour_num = int(rotate_queue[i]->neighbours.size());
        }
        int start = 0, end = int(groups.size());
        while (start != end) {
            VerboseGroup * g = rotate_queue[start++];
            if (g->remained_neighbour_num < COLOR_NUM) {
                sorted[tot_deleted++] = g;
                g->deleted = true;
                for (VerboseGroup * n: g->neighbours) {
                    if (n->deleted || n->remained_neighbour_num < COLOR_NUM)
                        continue;
                    n->remained_neighbour_num -= 1;
                    if (n->visited && n->remained_neighbour_num == COLOR_NUM - 1)
                    {
                        rotate_queue[end++] = n;

                    }
                }
            } 
            else {
                g->visited = true;
            }
            start %= (groups.size() + 1);
            end %= (groups.size() + 1);
        }
        delete rotate_queue;

        if (tot_deleted != (int)groups.size()) {
            return false;
        }

        for (int i = groups.size() - 1; i >= 0; --i) {
            int c = -1;
            VerboseGroup * g = sorted[i];
            for (int j = 0; j < COLOR_NUM; ++j) {
                // try color
                int try_c = (j + i % COLOR_NUM) % COLOR_NUM;
                for (auto n: g->neighbours) {
                    if (n->color == try_c) {
                        goto COLOR_FAILED;
                    }
                }
                c = try_c;
                break;
COLOR_FAILED:
                continue;
            }

            sorted[i]->color = c;
            sorted[i]->back_pointer->get_root_bucket()->group.color = c;
            if (c == -1) {
                printf("impossible\n");
                return false;
            }
        }

        for (VerboseGroup * g: sorted) {
            color_group(g->back_pointer, g->color);
        }
        return true;
    }

    bool try_color_all()
    {
        // collect group
        vector<VerboseGroup *> groups;
        unordered_map<void *, VerboseGroup *> dict;

        groups.clear();
        dict.clear();

        for (int i = 0; i < bucket_num; ++i) {
            VerboseBuckets * p = &v_buckets[i];
            if (dict.find(p->get_root_bucket()) == dict.end()) {
                auto vgp = dict[p->get_root_bucket()] = new VerboseGroup();
                groups.push_back(vgp);
                // group is linked to its root bucket, which is also the key of dict
                vgp->back_pointer = p->get_root_bucket();
                // create link between root_bucket and its group
                p->get_root_bucket()->group.back_pointer = p->get_root_bucket();
            }
        }

        for (int i = 0; i < bucket_num; ++i) {
            VerboseBuckets * p = &v_buckets[i];
            // Go through all pos_edges linked to the bucket
            for (CCEdge * e: p->pos_edges) {
                if (!e->available) continue;
                // the other node
                uint32_t other = e->get_other_val(uint32_t(i));
                auto vgp = dict[p->get_root_bucket()];
                // 这两个group之间有边，表示颜色应该不一致
                vgp->neighbours.insert(dict[v_buckets[other].get_root_bucket()]);
                // insert neighbours to group
                p->get_root_bucket()->group.neighbours.insert(&(v_buckets[other].get_root_bucket()->group));
            }
        }

        if (1) {
            unordered_map<void *, int> counter;
            for (int i = 0; i < bucket_num; ++i) {
                counter[v_buckets[i].get_root_bucket()]++;
            }
            int max_val = 0;
            for (auto itr: counter) {
                max_val = max(itr.second, max_val);
            }
            cout << "max size of a group / bucket num : " << max_val << "/" <<  bucket_num << endl;
            cout << "The size of group is "<< groups.size() << endl;
            // my code here
            // for(int i = 0; i< BUCKET_NUM; i++){
            //     cout << v_buckets[i].bucket_id << " "<<v_buckets[i].get_root_bucket()->bucket_id <<" "<<v_buckets[i].group.neighbours.size()<<endl;
            // }
        }

        bool result = try_color_groups(groups);

        // clear
        for (VerboseGroup * g: groups) {
            delete g;
        }

        return result;
    }

    static void color_group(VerboseBuckets * a, int color)
    {
        a = a->get_root_bucket();
        for (; a != NULL; a = a->next_bucket) {
            a->color = color;
        }
    }

    struct IncrementalTryColor
    {
        unsigned int tot_num;
        vector<VerboseGroup *> groups;
        vector<VerboseGroup *> sorted;
//        set<VerboseGroup *> remained_set;
        queue<VerboseGroup *> rotate_queue;

        IncrementalTryColor() : tot_num(0) {}

        void push_back(VerboseGroup * g)
        {
            rotate_queue.push(g);
            groups.push_back(g);
//            remained_set.insert(g);
            tot_num++;
        }

        bool run()
        {
            while (rotate_queue.size()) {
                VerboseGroup * g = rotate_queue.front();
                if (g->neighbours.size() - g->deleted_neighbour_num < COLOR_NUM) {
                    sorted.push_back(g);
//                    remained_set.erase(g);
                    g->deleted = true;
                    for (VerboseGroup * n: g->neighbours) {
                        if (n->deleted)
                            continue;
                        n->deleted_neighbour_num++;
                        if (n->visited && n->neighbours.size() - n->deleted_neighbour_num == COLOR_NUM - 1)
                            rotate_queue.push(n);
                    }
                } 
                else {
                    g->visited = true;
                }
                rotate_queue.pop();
            }

            if (sorted.size() != tot_num) {
                if (tot_num - sorted.size() < 32) {
//                    vector<VerboseGroup *> remained(remained_set.begin(), remained_set.end());
                    vector<VerboseGroup *> remained;
                    for (auto g: groups) {
                        if (!g->deleted) {
                            remained.push_back(g);
                        }
                    }
                    if (!try_color_groups_bf(remained)) {
                        return false;
                    }
                } 
                else {
                    return false;
                }
            }

            // if success, recolor
            // if failed, the color will not be affected
            color_sorted();

            return true;
        }

        // if run success, it will call color_sorted to recolor
        void color_sorted()
        {
            for (int i = sorted.size() - 1; i >= 0; --i) {
                int c = -1;
                VerboseGroup * g = sorted[i];
                for (int j = 0; j < COLOR_NUM; ++j) {
                    // try color
                    int try_c = (j + i % COLOR_NUM) % COLOR_NUM;
                    for (auto n: g->neighbours) {
                        if (n->color == try_c) {
                            goto COLOR_FAILED;
                        }
                    }
                    c = try_c;
                    break;
                    COLOR_FAILED:
                    continue;
                }

                sorted[i]->color = c;

                if (c == -1) {
                    printf("impossible\n");
                }
            }

            for (VerboseGroup * g: sorted) {
                color_group(g->back_pointer, g->color);
            }
        }
    };

    bool try_color_two_bucket(VerboseBuckets *a, VerboseBuckets *b = NULL)
    {
        IncrementalTryColor itc;

        // insert a, b to neighbour group
        unordered_set<VerboseGroup *> nbs, new_nbs;
        auto vpg = &(a->get_root_bucket()->group);
        nbs.insert(vpg);
        vpg->used = true;

        if (b) {
            vpg = &(b->get_root_bucket()->group);
            nbs.insert(vpg);
            vpg->used = true;
        }

        bool success = false;
        int turn = 0;

        while (1) {
            turn += 1;
            
            // go throuth the whole graph
            if (nbs.size() == 0) {
                // cout << turn << " " << itc.tot_num << " " << itc.sorted.size() << endl;
                success = false;
                break;
            }

            // add neighbour into groups
            for (auto p: nbs) {
                itc.push_back(p);
                p->color = -1;
            }

            // create new neighbours
            for (VerboseGroup * p: nbs) {
                for (VerboseGroup * n: p->neighbours) {
                    if (!n->used) {
                        new_nbs.insert(n);
                        n->color = n->back_pointer->color;
                        n->used = true;
                    }
                }
            }

            nbs.clear();
            nbs.swap(new_nbs);

            bool result = itc.run();
//            bool result;
//            if (itc.groups.size() < 15) {
//                result = try_color_groups_bf(itc.groups);
//            }
//            if (!result) {
//                result = itc.run();
//            }

            if (result) {
                success = true;
                break;
            }
        }

        for (auto g: nbs) {
            g->deleted = false;
            g->used = false;
            g->visited = false;
            g->deleted_neighbour_num = 0;
        }

        // clear
        node_num_to_update = 0;
        UpdateCC.clear();
        for (VerboseGroup * g: itc.groups) {
            g->deleted = false;
            g->used = false;
            g->visited = false;
            g->deleted_neighbour_num = 0;
            for (auto p = (g->back_pointer); p; p = p->next_bucket) {
                // synchronize(p - v_buckets);
                UpdateCC.insert(p->bucket_id);
                affected_node_num += 1;
                node_num_to_update += 1;
            }
        }
        
        // failed
        if(!success){
            return success;
        }
        
        // UpdateCC.dis();
        UpdateCC.update(this);
        return success;
    }

    // a pos edge linked these two groups means that we cannot insert a negedge between a and b
    bool check_two_group_have_collision_edge(VerboseBuckets * a, VerboseBuckets * b)
    {
        for (; b != NULL; b = b->next_bucket) {
            for (CCEdge * e: b->pos_edges) {
                if (!e->available) continue;
                int idx = e->get_other_val(b - v_buckets);
                if (v_buckets[idx].get_root_bucket() == a) {
                    return true;
                }
            }
        }
        return false;
    }

public:
    ColoringClassifier() {
        name = "CC" + string(1, char('0' + COLOR_NUM));
        edge_collision_num = 0;
        affected_node_num = 0;
        memset(buckets, 0, sizeof(buckets));
        delete hash1;
        delete hash2;
        hash1 = NULL;
        hash2 = NULL;
        if (!hash1) {
            srand(time(0));
            hash1 = new BOBHash(rand());
            hash2 = new BOBHash(rand());
        }
        for(int i = 0; i < BUCKET_NUM; i++){
            v_buckets[i].bucket_id = i;
        }
    };

    void random_set_hash(){
        srand(time(0));
        if (hash1) {
            delete hash1;
            hash1 = NULL;
        }
        if (hash2) {
            delete hash2;
            hash2 = NULL;
        }
        hash1 = new BOBHash(rand());
        hash2 = new BOBHash(rand());
    }
    
    void random_set_hash(uint32_t offset){
        srand(time(0) + 1000 * offset);
        if (hash1) {
            delete hash1;
            hash1 = NULL;
        }
        if (hash2) {
            delete hash2;
            hash2 = NULL;
        }
        hash1 = new BOBHash(rand());
        // cout << hash1 -> seed << endl;
        hash2 = new BOBHash(rand());
        // cout << hash2 -> seed << endl;
    }
    // INT to construct the CC
    void set_pos_edge(uint64_t * items, int num) {
        pos_edges.resize(num);
        for (int i = 0; i < num; ++i) {
            pos_edges[i] = new CCEdge(items[i]);
        }
    };
    // STR to construct the CC
    void set_pos_edge(const char items[][MAX_LEN], int num) {
        pos_edges.resize(num);
        for (int i = 0; i < num; ++i) {
            pos_edges[i] = new CCEdge(items[i]);
        }
    }

    void set_neg_edge(uint64_t * items, int num) {
        neg_edges.resize(num);
        for (int i = 0; i < num; ++i) {
            neg_edges[i] = new CCEdge(items[i]);
        }
    }

    void set_neg_edge(const char items[][MAX_LEN], int num) {
        neg_edges.resize(num);
        for (int i = 0; i < num; ++i) {
            neg_edges[i] = new CCEdge(items[i]);
        }
    }

    bool build() {
        // use neg unordered_set build group
        collision_time = 0;
        while(collision_time < MAX_EDGE_COLLISION_TIME){
            if(verbose){
                printf("Build for the %d time...\n", collision_time+1);
            }
            // 初始化edge_colision_num
            edge_collision_num = 0;
            if(collision_time != 0){
                // initialize what has been changed
                for (int i = 0; i < BUCKET_NUM; i++){
                    auto cur_bucket = &v_buckets[i];
                    cur_bucket->pos_edges.clear();
                    cur_bucket->neg_edges.clear();

                    cur_bucket->root_bucket = cur_bucket;
                    cur_bucket->next_bucket = NULL;
                    cur_bucket->last_son = cur_bucket; // only useful when root

                    cur_bucket->group.back_pointer = cur_bucket;
                }
                // reset hash
                random_set_hash(collision_time);
                // printf("rehash for neg_edge...\n");
                for (auto & edge: neg_edges){
                    CCEdge * e = edge;
                    e->available = true;
                    // cout << e->hash_val_a <<" "<< e->hash_val_b << endl;
                    e->set_hash_val(e->e);
                    // cout << e->hash_val_a <<" "<< e->hash_val_b << endl;
                }
                // printf("rehash for pos_edge...\n");
                for (auto & edge: pos_edges){
                    CCEdge * e = edge;
                    e->available = true;
                    // cout << e->hash_val_a <<" "<< e->hash_val_b << endl;
                    e->set_hash_val(e->e);
                    // cout << e->hash_val_a <<" "<< e->hash_val_b << endl;
                }
            }
            
            // two bucket linked with negedge will be set same color 
            if (verbose) fprintf(stderr, "set neg edge.\n");
            for (auto & edge: neg_edges) {
                CCEdge * e = edge;
                auto bucket_a = &v_buckets[e->hash_val_a];
                auto bucket_b = &v_buckets[e->hash_val_b];

                bucket_a->neg_edges.push_back(e);
                bucket_b->neg_edges.push_back(e);

                if (bucket_a->get_root_bucket() != bucket_b->get_root_bucket()) {
                    bucket_b->set_root_bucket(bucket_a->get_root_bucket());
                }
            }

            // check pos unordered_set available
            if (verbose) fprintf(stderr, "set pos edge.\n");
            for (auto & edge: pos_edges) {
                CCEdge * e = edge;
                auto bucket_a = &v_buckets[e->hash_val_a];
                auto bucket_b = &v_buckets[e->hash_val_b];

                bucket_a->pos_edges.push_back(e);
                bucket_b->pos_edges.push_back(e);

                // edge collision means that there is certainly an error at last
                if (bucket_a->get_root_bucket() == bucket_b->get_root_bucket()) {
                    edge_collision_num += 1;
                    e->available = false;
                    // 1 for posedge
                    OverFlowTable.insert(e->e, 1);
                }
            }
            // at the begining of the while loop, initial what has been changed
            if(edge_collision_num != 0){
                collision_time ++;
                continue;
            }
            if(edge_collision_num == 0){
                break;
            }
        }
        
        // If there's still edge_collision after MAX_EDGE_COLLISION_TIME,
        // we ignore the collision and endure some error.
        bool color_result = try_color_all();
        if (!color_result) {
            return false;
        }
        synchronize_all();

        return true;
    }

// #define insertDubug
    bool insert(uint64_t item, int class_id){
        // for(int i = 0; i < BUCKET_NUM; i++){
            // old_buckets[i] = v_buckets[i];
            // old_buckets[i].root_bucket = &old_buckets[v_buckets[i].root_bucket-v_buckets];
            // old_buckets[i].next_bucket = (v_buckets[i].next_bucket==NULL)?NULL:&old_buckets[v_buckets[i].next_bucket-v_buckets];
            // old_buckets[i].last_son = &old_buckets[v_buckets[i].last_son-v_buckets];   
            // old_buckets[i].group.back_pointer = &old_buckets[v_buckets[i].group.back_pointer-v_buckets];
        // }
        
        if (class_id == 1) {
            // if insert an pos edge
            CCEdge * e = new CCEdge(item);
            pos_edges.push_back(e);

            auto bucket_a = &v_buckets[e->hash_val_a];
            auto bucket_b = &v_buckets[e->hash_val_b];

            bucket_a->pos_edges.push_back(e);
            bucket_b->pos_edges.push_back(e);

            auto root_a = bucket_a->get_root_bucket();
            auto root_b = bucket_b->get_root_bucket();

            // edge collision and error
            if (root_a == root_b) {
                e->available = false;
                edge_collision_num += 1;
                OverFlowTable.insert(item, class_id);
                #ifdef insertDebug
                cout << "Pos edge with edge collision has been inserted into oftable." <<endl;
                #endif
                return true;
            }

            root_a->group.neighbours.insert(&(root_b->group));
            root_b->group.neighbours.insert(&(root_a->group));

            // if the colors of the two buckets is diffrent, we don't need to do anything
            if (bucket_a->color != bucket_b->color) {
                #ifdef insertDebug
                cout << "Do nothing." << endl;
                #endif
                return true;
            }
            
            // if the color of the two buckets is the same, we try recolor the graph
            #ifdef insertDebug
            cout <<"Pos Edge Recolor." <<endl;
            #endif
            
            bool flag = try_color_two_bucket(bucket_a, bucket_b);
            if(flag){
                return flag;
            }
            // failed
            else{
                cout << "Pos edge recolor failed." <<endl;
                e->available = false;
                edge_collision_num += 1;
                OverFlowTable.insert(item, class_id);
                
                root_a->group.neighbours.erase(&(root_b->group));
                root_b->group.neighbours.erase(&(root_a->group));

                return flag;
            }
        } 
        else {
            neg_edges.push_back(new CCEdge(item));
            CCEdge * e = neg_edges.back();

            auto bucket_a = &v_buckets[e->hash_val_a];
            auto bucket_b = &v_buckets[e->hash_val_b];

            bucket_a->neg_edges.push_back(e);
            bucket_b->neg_edges.push_back(e);

            // in the same group, we don't do anything
            if (bucket_a->get_root_bucket() == bucket_b->get_root_bucket()) {
                #ifdef insertDebug
                cout << "Do nothing." << endl;
                #endif
                return true;
            }

            // check whether there is a pos edge in these two group
            // edge collision and error
            if (check_two_group_have_collision_edge(bucket_a->get_root_bucket(), bucket_b->get_root_bucket())) {
                e->available = false;
                edge_collision_num += 1;
                OverFlowTable.insert(item, class_id);
                #ifdef insertDebug
                cout <<"neg edge with edge collision has been inserted into oftable." <<endl;
                #endif
                
                return true;
            }

            auto root_a = bucket_a->get_root_bucket();
            auto root_b = bucket_b->get_root_bucket();
            auto inteval = root_a->last_son;

            if (bucket_a->color == bucket_b->color) {
                for (VerboseGroup * n: root_b->group.neighbours) {
                    n->neighbours.erase(&(root_b->group));
                    n->neighbours.insert(&(root_a->group));
                    root_a->group.neighbours.insert(n);
                }
                bucket_b->set_root_bucket(root_a);
                #ifdef insertDebug
                cout <<"Do nothing." <<endl;
                #endif
                return true;
            }

            // 复制旧的group
            for(int i = 0; i < BUCKET_NUM; i++){
                old_buckets[i].group = v_buckets[i].group;
            }

            // 把root_b所有的邻居从b上erase掉之后挂载root_a上
            for (VerboseGroup * n: root_b->group.neighbours) {
                n->neighbours.erase(&(root_b->group));
                n->neighbours.insert(&(root_a->group));
                root_a->group.neighbours.insert(n);
            }

            // reset all group pointer
            // 这里set root bucket会导致整个图都改变，但应该更精细化地恢复状态
            bucket_b->set_root_bucket(root_a);

            #ifdef insertDebug
            cout << "Neg Edge Recolor" << endl;
            #endif
            bool flag = try_color_two_bucket(bucket_a);
            if(flag){
                return flag;
            }
            else{
                cout << "Neg edge recolor failed." <<endl;
                e->available = false;
                edge_collision_num += 1;
                OverFlowTable.insert(item, class_id);
                
                int mode = 0;
                root_b->last_son = root_a->last_son;
                root_a->last_son = inteval;
                for(auto it = root_a; it!=NULL; it = it->next_bucket){
                    if(mode == 0){
                        it->root_bucket = root_a;
                    }
                    else if(mode == 1){
                        it->root_bucket = root_b;
                    }
                    if(it == inteval){
                        mode = 1;
                    }
                }
                inteval->next_bucket = NULL;
        
                // for(int i = 0; i < BUCKET_NUM; i++){
                    // int i = aff[j];
                    // cout <<i<<": " <<endl;
                    // cout << v_buckets[i].bucket_id <<" " <<((v_buckets[i].next_bucket!=NULL)?(v_buckets[i].next_bucket->bucket_id):-1)<<" "<<v_buckets[i].last_son->bucket_id <<" "<< endl;
                    // cout << old_buckets[i].bucket_id <<" " <<((old_buckets[i].next_bucket!=NULL)?(old_buckets[i].next_bucket->bucket_id):-1) <<" "<<old_buckets[i].last_son->bucket_id<<" " << endl;
                    // if(v_buckets[i].get_root_bucket()->bucket_id != old_buckets[i].get_root_bucket()->bucket_id) cout <<"Unequal!"<<endl;
                // }
                // for(int j = 0; j < aff.size(); j++){
                    // cout << aff.size() << endl;
                    // int i = aff[j];
                    // v_buckets[i].group = old_buckets[i].group;
                    // v_buckets[i].root_bucket = &v_buckets[old_buckets[i].root_bucket-old_buckets];
                    // v_buckets[i].next_bucket = (old_buckets[i].next_bucket==NULL)?NULL:&v_buckets[old_buckets[i].next_bucket-old_buckets];
                    // v_buckets[i].last_son = &v_buckets[old_buckets[i].last_son-old_buckets];
                // }

                // we only need to reset for group
                for(int i = 0; i < BUCKET_NUM; i++){
                    v_buckets[i].group = old_buckets[i].group;
                }
                return flag;
            }
        }
    }

    int query(uint64_t item) {
        CCEdge e;
        e.set_hash_val(item);

        int c1, c2;
        c1 = get_bucket_val(e.hash_val_a);
        c2 = get_bucket_val(e.hash_val_b);

        return c1 == c2;
    }

    int query(const char * item){
        CCEdge e;
        e.set_hash_val(item);

        int c1, c2;
        c1 = get_bucket_val(e.hash_val_a);
        c2 = get_bucket_val(e.hash_val_b);

        return c1 == c2;
    }

    // 直接构造keys，values是一半一半
    bool exp_build(uint64_t * keys, int num){
        set_pos_edge(keys, num / 2);
        set_neg_edge(keys + num / 2, num - (num / 2));

        return build();
    }

    // 初始化，设置v_buckets的color
    void init(){
        for (int i = 0; i < bucket_num; ++i) {
            v_buckets[i].color = 0;
        }
    }

    void report()
    {
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
            "report coloring result...\n");
        printf("the summary of color embedder:\n"
        "\tthe edge collision num is %d\n"
        // "\tthe edge collision time is %d\n"
        "\tthe neg item num is %d\n"
        "\tthe pos item num is %d\n"
        "\tThe size of overflow table is %d\n"
        "\treport summary done.\n", edge_collision_num, 
        // collision_time, 
        (int)neg_edges.size(), (int)pos_edges.size(), OverFlowTable.size());        
        printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
                
        // for (int i = 0; i < bucket_num; ++i) {
        //     if(i%(bucket_num/10)) continue;
        //     cout << i << ": " << v_buckets[i].color << " ";
        //     for (CCEdge * e: v_buckets[i].pos_edges) {
        //         cout << "(" << e->hash_val_a << "," << e->hash_val_b << ") ";
        //     }
        //     cout << endl;
        // }
    }

    ~ColoringClassifier() {
        for (auto e: pos_edges) {
            delete e;
        }
        for (auto e: neg_edges) {
            delete e;
        }
    }
};

#endif //COLORINGCLASSIFER_COLORING_CLASSIFIER_H
