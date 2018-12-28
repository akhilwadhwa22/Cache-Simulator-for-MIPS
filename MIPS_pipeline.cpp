/*
Cache Simulator
Level one L1 and level two L2 cache parameters are read from file (block size, line per set and set per cache).
The 32 bit address is divided into tag bits (t), set index bits (s) and block offset bits (b)
s = log2(#sets)   b = log2(block size)  t=32-s-b
*/
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <stdlib.h>
#include <cmath>
#include <bitset>
#include <math.h>
# include <cstring>

using namespace std;
//access state:
#define NA 0 // no action
#define RH 1 // read hit
#define RM 2 // read miss
#define WH 3 // Write hit
#define WM 4 // write miss

struct config{
       int L1blocksize;
       int L1setsize;
       int L1size;
       int L2blocksize;
       int L2setsize;
       int L2size;
       };

// you can define the cache class here, or design your own data structure for L1 and L2 cache

class Utils {
    public:
    void print_vector(vector<int> v){
        for(int i = 0; i < v.size(); i++){
            cout<<v[i];
        }
        cout<<endl;
    }
    long unsigned toInt(vector<int> binary) {
        int sum = 0;
        for (int i = 0; i < binary.size(); i++){
            sum += (pow(2,i))*binary[binary.size()-i-1];
            }
        return sum;
        }
    };

Utils utils;

class cache {
    public:
    int n_sets;
    int n_index_bits;
    int n_offset_bits;
    int n_tag_bits;
    int power_b;
    int power_a;
    int tag_match;
    bool valid_bit;
    vector<int> offset;
    vector<int> tags;
    vector<int> index;
    unsigned long tag_int;
    unsigned long index_int;
    unsigned long offset_int;
    bool read_miss;
    unsigned long counter_c;
    unsigned long counter_r;
    vector < vector < unsigned long > > Mylru;

   void computeN_Bits(int c_size,int associativity, int b_size){
        if(associativity>0){
            n_sets=(c_size*1024)/(b_size*associativity);
            n_index_bits=log2(n_sets);
        }

        else if(associativity==0){
            n_sets=c_size*1024;
            n_index_bits=0;
            }

        n_offset_bits = log2(b_size);
        n_tag_bits=32-n_offset_bits-n_index_bits;
        }

    void computePowerA(int block_size) {
        power_a = checkPow(block_size);
        }

    void computePowerB(int block_size) {
        power_b = checkPow(block_size);
        }

    void computeBits(int n_tag_bits,int n_index_bits,int n_offset_bits,bitset<32> addrs){
        offset.clear();
        index.clear();
        tags.clear();
        for(int i=n_offset_bits-1; i>=0; i--){
            offset.push_back(addrs[i]);      //computing the offset bits
        }
        for(int i=n_offset_bits+n_index_bits-1;i>=n_offset_bits;i--){    //computing the index bits
            index.push_back(addrs[i]);
            }

        for(int i=31;i>=n_offset_bits+n_index_bits;i--){   //computing the tag bits
            tags.push_back(addrs[i]);
        }
        n_offset_bits=31-n_offset_bits;

        }

    private:
        int checkPow(int block_size){    //power check function
            int x;
            if(block_size>0){
                if(block_size==1){
                    x= 1;
                    }
                else if(ceil(log2(block_size))==floor(log2(block_size))){   //check if the block size is a power of 2
                    x= log2(block_size);
                    }
                else
                    x= 0;
            }

            return x;
        }

};

int test(int argc, char* argv[]){

    string tags,index,offset;
    unsigned long c1,c2,i;
    cache l1,l2;
    int n=1;
    l1.counter_c=0;
    l2.counter_c=0;

    l1.counter_r=0;
    l2.counter_r=0;


    config cacheconfig;         //cacheconfig is an of object of struct config
    ifstream cache_params;     // input stream
    string dummyLine;
    cache_params.open("cacheconfig.txt");

    while(!cache_params.eof()){  // read config file //reads the parameters from the command line.
      cache_params>>dummyLine;
      cache_params>>cacheconfig.L1blocksize;
      cache_params>>cacheconfig.L1setsize;
      cache_params>>cacheconfig.L1size;
      cache_params>>dummyLine;
      cache_params>>cacheconfig.L2blocksize;
      cache_params>>cacheconfig.L2setsize;
      cache_params>>cacheconfig.L2size;
      }

      l1.computePowerB(cacheconfig.L1blocksize);
      l1.computePowerA(cacheconfig.L1setsize);

      if(l1.power_b!=0 && l1.power_a!=0){
        l1.computeN_Bits(cacheconfig.L1size,cacheconfig.L1setsize,cacheconfig.L1blocksize);
        }

      else
        cout<<"ERROR in powers of L1"<<endl;

      l2.computePowerB(cacheconfig.L2blocksize);
      l2.computePowerA(cacheconfig.L2setsize);

      if(l2.power_b!=0 && l2.power_a!=0){
        l2.computeN_Bits(cacheconfig.L2size,cacheconfig.L2setsize,cacheconfig.L2blocksize);
        }
      else
        cout<<"ERROR in powers of L2"<<endl;

    if(cacheconfig.L1setsize == 0){
        cacheconfig.L1setsize=l1.n_sets;
        l1.n_sets=1;

    }
    if( cacheconfig.L2setsize == 0 ){
        cacheconfig.L2setsize=l2.n_sets;
        l2.n_sets=1;
    }

    int L1block[l1.n_sets][2*cacheconfig.L1setsize];
    int L2block[l2.n_sets][2*cacheconfig.L2setsize];
    l1.Mylru.resize(l1.n_sets);

    l2.Mylru.resize( l2.n_sets);

    int L1AcceState =0; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState =0; // L2 access state variable, can be one of NA, RH, RM, WH, WM;

    ifstream traces;
    ofstream tracesout;
    string outname;
    outname = string("trace_small.txt") + ".out";

    traces.open("trace_small.txt");

    tracesout.open(outname.c_str());

    string line;
    string accesstype;
    string xaddr;
    unsigned int addr;
    bitset<32> accessaddr;

    if (traces.is_open()&&tracesout.is_open()){

        while (getline (traces,line)){
            istringstream iss(line);
            if (!(iss >> accesstype >> xaddr)) {break;}
            stringstream saddr(xaddr);
            saddr >> std::hex >> addr;
            accessaddr = bitset<32> (addr);

            l1.computeBits(l1.n_tag_bits,l1.n_index_bits,l1.n_offset_bits,accessaddr);//void computeBits(int n_tag_bits,int n_index_bits,n_offset_bits,string addrs){
            l1.tag_int=utils.toInt(l1.tags);
            l1.offset_int=utils.toInt(l1.offset);
            l1.index_int=utils.toInt(l1.index);

            l2.computeBits(l2.n_tag_bits,l2.n_index_bits,l2.n_offset_bits,accessaddr);
            l2.tag_int=utils.toInt(l2.tags);
            l2.offset_int=utils.toInt(l2.offset);
            l2.index_int=utils.toInt(l2.index);

//////////////       L1 read      //////////////////
            if (accesstype.compare("R")==0){
                for(l1.counter_c=0;l1.counter_c<2*cacheconfig.L1setsize;l1.counter_c+=2){ //checking the tag bits
                    if(L1block[l1.index_int][l1.counter_c]==l1.tag_int){
                        l1.tag_match=1;
                        c1=l1.counter_c;
                        break;
                        }
                    }
            if(l1.tag_match==1){                   //checking for a tag match
                if(L1block[l1.index_int][c1+1]==1){
                    l1.valid_bit=1;
                    }
                }

            if(l1.valid_bit==1 && l1.tag_match==1 ){
                L1AcceState=RH;
                l1.Mylru[l1.index_int].push_back(c1);
                }

            else{
                L1AcceState=RM;
                for (l1.counter_c=1;l1.counter_c<(2*cacheconfig.L1setsize);l1.counter_c+=2){
                    L1block[l1.index_int][l1.counter_c-1]=l1.tag_int;
                    L1block[l1.index_int][l1.counter_c]=1;
                    break;
                    }
                }

//////////////////////          L2 Read      ///////////////////////////////
             if (L1AcceState==RH){
                L2AcceState=NA;
             }
             else if (L1AcceState==RM){
                for(l2.counter_c=0;l2.counter_c<2*cacheconfig.L2setsize;l2.counter_c+=2){    //checking the tag bits
                    if(L2block[l2.index_int][l2.counter_c]==l2.tag_int){
                        l2.tag_match=1;
                        c2=l2.counter_c;
                        break;
                        }
                    }

                if(l2.tag_match==1){
                    if(L2block[l2.index_int][c1+1]==1){
                        l2.valid_bit=1;
                        }
                    }

                if(l2.valid_bit==1 && l2.tag_match==1){
                    L2AcceState=RH;
                    L1block[l1.index_int][c1+1]=1;            // Update L1 as well
                    L1block[l1.index_int][c2]=l2.tag_int;
                    }

                else if(l1.valid_bit!=1 || l1.tag_match!=1 ){
                    L2AcceState=RM;
                    for(l2.counter_c=1;l2.counter_c<(2*cacheconfig.L2setsize);l2.counter_c+=2){
                        L2block[l2.index_int][l2.counter_c]=1;
                        L2block[l2.index_int][l2.counter_c-1]=l2.tag_int;
                        break;
                        }
                    }
                }
            }
///////////////////////     L1 Write     ////////////
             else{
                for(l1.counter_c=0;l1.counter_c<2*cacheconfig.L1setsize;l1.counter_c+=2){ //checking the tag bits
                    if(L1block[l1.index_int][l1.counter_c]==l1.tag_int){
                        l1.tag_match=1;
                        c1=l1.counter_c;
                        break;
                        }
                    }

                if(l1.tag_match==1){
                    if(L1block[l1.index_int][c1+1]==1){
                        l1.valid_bit=1;
                        }
                    }

                if(l1.valid_bit==1 && l1.tag_match==1 ){
                    L1AcceState=WH;
                    for (l1.counter_c=1;l1.counter_c<(2*cacheconfig.L1setsize);l1.counter_c+=2){
                        L1block[l1.index_int][l1.counter_c-1]=l1.tag_int;
                        L1block[l1.index_int][l1.counter_c]=1;
                        }

                    for (l2.counter_c=1;l2.counter_c<(2*cacheconfig.L2setsize);l2.counter_c+=2){
                        L2block[l2.index_int][l2.counter_c-1]=l2.tag_int;
                        L2block[l2.index_int][l2.counter_c]=1;
                        break;
                        }
                    }

                else
                    L1AcceState=WM;
////////////////////// L2 Write  /////////////////////////////////////////////////

                for(l2.counter_c=0;l2.counter_c<2*cacheconfig.L2setsize;l2.counter_c+=2){ //checking the tag bits
                    if(L2block[l2.index_int][l2.counter_c]==l2.tag_int){
                        l2.tag_match=1;
                        c2=l2.counter_c;
                        break;
                        }
                    }

                if(l2.tag_match==1){
                    if(L2block[l2.index_int][c2+1]==1){
                        l2.valid_bit=1;
                        }
                    }
                if(l2.valid_bit==1 && l2.tag_match==1 ){
                    L2AcceState=WH;
                    for (l2.counter_c=1;l2.counter_c<(2*cacheconfig.L2setsize);l2.counter_c+=2){
                        L2block[l2.index_int][l2.counter_c-1]=l2.tag_int;
                        L2block[l2.index_int][l2.counter_c]=1;
                        break;
                        }
                    }
                else
                    L2AcceState=WM;
                }

            tracesout<< L1AcceState << " " << L2AcceState << endl;  // Output hit/miss results for L1 and L2 to the output file;
            l1.tag_match=0;
            l1.read_miss=0;
            l2.tag_match=0;
            l2.read_miss=0;
        }
        traces.close();
        tracesout.close();
    }
    else cout<< "Unable to open trace or traceout file ";

    return 0;
}

int main() {
    string temp = "cacheconfig.txt";
    char * tab2 = new char [temp.length()+1];
    strcpy (tab2, temp.c_str());
    test(1, &tab2);
}
