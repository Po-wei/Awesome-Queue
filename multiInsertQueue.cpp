#include <iostream>
#include <atomic>
#include <thread>
#include <functional>
using namespace std;

static const uint64_t MARK_BIT = 0xaa000000000000;
static const uint64_t MASK     = 0x00FFFFFFFFFFFF;

static const int TIMES = 10000;
static const int NUM_THREAD = 16; 

class Node
{
public:
    Node()
    :Node(-1)
    {
    }

    Node(int v)
    :val(v)
    {
    }


    int val;
    atomic<Node*> next; 
};


class QueueInterface
{
public:
    virtual void push(int val) = 0;
    virtual bool pop(int &ret) = 0;

public:
    atomic<Node*> head;
    atomic<Node*> tail;
};


class MSQueue
{
public:
    MSQueue()
    {
        Node* dummy = new Node();
        dummy->next.store(nullptr);
        head.store(dummy);
        tail.store(dummy);
    }

    MSQueue(const MSQueue& ms)
    {
        Node* dummy = new Node();
        dummy->next.store(nullptr);
        head.store(dummy);
        tail.store(dummy);
        // SHOULD NOT BE CALLED
    }


    void push(int v)
    {
        Node* newNode = new Node(v);
        newNode->next.store(nullptr);
        Node* localTail;

        while(true)
        {
            localTail = this->tail.load();
            Node* localTailNext = localTail->next.load();
            if( localTail == this->tail.load())
            {
                if(localTailNext == nullptr)
                {
                    // do the same thing as MS queue
                    if( localTail->next.compare_exchange_strong(localTailNext, newNode))
                    {
                        break;
                    }
                }
                else
                {
                    this->tail.compare_exchange_strong(localTail, localTailNext);
                }
            }
        }
        this->tail.compare_exchange_strong(localTail, newNode);
    }


    bool pop(int &ret)
    {
        while(true)
        {
            Node* localHead = this->head.load();
            Node* localTail = this->tail.load();
            Node* localHeadNext = localHead->next.load();
            if( localHead == this->head.load())
            {
                if( localHead == localTail) // empty
                {
                    if( localHeadNext == nullptr)
                    {
                        return false;
                    }
                    else
                    {
                        // help advance the tail
                        this->tail.compare_exchange_strong(localTail, localHeadNext);
                    }
                }
                else
                {
                    ret = localHeadNext->val;
                    if(this->head.compare_exchange_strong( localHead, localHeadNext))
                    {
                        break;
                    }
                }
                
            }
        }
        return true;
    }

public:
    atomic<Node*> head;
    atomic<Node*> tail;
};



class AwesomeQueue
{
public:
    AwesomeQueue()
    {
        Node* dummy = new Node();
        dummy->next.store(setMark(nullptr));
        head.store(dummy);
        tail.store(dummy);
    }


    void push(int v)
    {
        Node* newNode = new Node(v);
        newNode->next.store(nullptr);
        Node* localTail;

        // pointer here may contain mark
        // always clear the mark first
        while(true)
        {
            localTail = this->tail.load();
            Node* localTailNext = localTail->next.load();
            if( localTail == this->tail.load())
            {
                // only one mark bit does not matter
                if(clearMark(localTailNext) == nullptr)
                {
                    // do the same thing as MS queue
                    if( localTail->next.compare_exchange_strong(localTailNext, newNode))
                    {
                        this->tail.compare_exchange_strong(localTail, newNode);
                        return;
                    }
                }

                //Tail always store clear-mark version.
                this->tail.compare_exchange_strong(localTail, clearMark(localTailNext));
                // Some excited stuff here!

                for(int i = 0 ; i < NUM_THREAD-1 ; i++)
                {
                    localTailNext = localTail->next.load();
                    if(isDeleted(localTailNext))
                    {
                        cout << "DELETED!!" << endl;
                        break;
                    }
                    newNode->next.store(localTailNext);
                    if(localTail->next.compare_exchange_strong(localTailNext, newNode))
                    {
                        return;
                    }
                }
                cout << "WRONG!!" << endl;
            }
        }
    }

    bool pop(int& ret)
    {
        while(true)
        {
           Node* localHead = this->head.load();
           Node* localHeadNext = localHead->next.load();
           if(localHead == this->head.load())
           {
               if( isDeleted(localHeadNext))
               {
                   if(clearMark(localHeadNext) == nullptr)
                   {
                       return false;
                   }
                   this->head.compare_exchange_strong(localHead, clearMark(localHeadNext));
                   continue;
               }
               else if(localHead->next.compare_exchange_strong(localHeadNext, setMark(localHeadNext)))
               {
                   ret = localHead->val;
                   return true;
               }
           }
        }
    }


    static Node* clearMark(Node* addr) 
    {
        return reinterpret_cast<Node*>((reinterpret_cast<uint64_t>(addr) & MASK));
    }

    static Node* setMark(Node* addr)
    {
        return reinterpret_cast<Node*>((reinterpret_cast<uint64_t>(addr) | MARK_BIT));
    }

    static bool isDeleted(Node* addr)
    {
        return reinterpret_cast<uint64_t>(addr) & MARK_BIT;
    }

public:
    atomic<Node*> head;
    atomic<Node*> tail;
};

void insertTestA(AwesomeQueue &ms)
{
    int r;
    for(int i = 0 ; i < TIMES ; i++)
    {
        // ms.push(i);
        ms.pop(r);
        // cout << ms.head << " " << ms.tail <<endl; 
    }
}

void insertTestB(AwesomeQueue &ms)
{
    for(int i = 0 ; i < TIMES ; i++)
    {
        ms.push(-i);
        
    }
}

void insertTestC(AwesomeQueue &ms)
{
    int r;
    for(int i = 0 ; i < TIMES ; i++)
    {
        ms.pop(r);
    }
}



int main()
{
    AwesomeQueue ms;

    thread t1(insertTestA, ref(ms));
    thread t2(insertTestB, ref(ms));
    thread t3(insertTestC, ref(ms));

    
    t1.join();
    t2.join();
    t3.join();
    // for(int i = 0 ; i < TIMES ; i++)
    // {
    //     ms.push(i);
    // }


    int r;
    int cnt = 0;
    // while(ms.pop(r))
    // {
    //     cnt++;
    //     cout << "HE" <<endl;
    // }
   

    cout << "Number of items in the queue: " << cnt << endl;

    return 0;
}