#include<iostream>
#include<vector>
using namespace std;
// static const long long MARK = 0x1;
static const long long MARK = 0xAA000000000000;
static const long long MASK = 0x00FFFFFFFFFFFF;

int main()
{
    vector<int*> addr;
    for(int i = 0 ; i < 10000000 ; i++)
    {
        int *a = new int (i);
        // addr.push_back( (int*)((size_t)a | MARK));
        cout << a << " : "  << endl;;
    }

    // for(auto i : addr)
    // {
    //     int* ptr = (int*)((size_t)i & MASK);
    //     cout << *ptr << endl;
    // }
    // cout << sizeof(size_t) << endl;
    // cout << sizeof(int) << endl;
    // cout << sizeof(long) << endl;
    // cin.get();
}