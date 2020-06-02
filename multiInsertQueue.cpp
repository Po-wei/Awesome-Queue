#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <functional>
using namespace std;

static const uint64_t MARK_BIT = 0xaa000000000000;
static const uint64_t MASK     = 0x00FFFFFFFFFFFF;

static const int TIMES = 100000;
static const int NUM_THREAD = 16; 

class Node
{
public:
    Node()
    :Node(-1) 
    {}

    Node(int v)
    :val(v) 
    {}

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


class MSQueue : public QueueInterface
{
public:
    MSQueue()
    {
        cout << "Constructing MSQueue" << endl;
        Node* dummy = new Node();
        dummy->next.store(nullptr);
        head.store(dummy);
        tail.store(dummy);
    }

    void push(int v) override
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


    bool pop(int &ret) override
    {
        while(true)
        {
            Node* localHead = this->head.load();
            Node* localTail = this->tail.load();
            Node* localHeadNext = localHead->next.load();
            if( localHead == this->head.load())
            {
                if( localHead == localTail)  //empty
                {
                    if( localHeadNext == nullptr)
                    {
                        return false;
                    }
                    else // help advance the tail
                    {
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
};



class AwesomeQueue : public QueueInterface
{
public:
    AwesomeQueue()
    {
        cout << "Constructing AwesomeQueue" << endl;
        Node* dummy = new Node();
        dummy->next.store(setMark(nullptr));
        head.store(dummy);
        tail.store(dummy);
    }


    void push(int v) override
    {
        Node* newNode = new Node(v);
        newNode->next.store(nullptr);
        Node* localTail;
        Node* localTailNext;
        // pointer here may contain mark
        // always clear the mark first
        while(true)
        {
            localTail = this->tail.load();
            localTailNext = localTail->next.load();
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
                localTailNext = localTail->next.load();
                this->tail.compare_exchange_strong(localTail, clearMark(localTailNext));
                // Some excited stuff here!
                // Insert in the middle
                for(int i = 0 ; i < NUM_THREAD-1 ; i++)
                {
                    localTailNext = localTail->next.load();
                    if(isDeleted(localTailNext))
                    {
                        break;
                    }
                    newNode->next.store(localTailNext);
                    if(localTail->next.compare_exchange_strong(localTailNext, newNode))
                    {
                        return;
                    }
                }
            }
        }
    }

    bool pop(int& ret)  override
    {
        while(true)
        {
            Node* localHead = this->head.load();
            Node* localTail = this->tail.load();
            Node* popCandidate = clearMark(localHead->next.load());

            if( localHead == this->head.load())
            {
                if( localHead == localTail) // empty
                {
                    if( popCandidate == nullptr)
                    {
                        return false;
                    }
                    else
                    {
                        this->tail.compare_exchange_strong(localTail, popCandidate);
                    }
                }
                else if(isDeleted(popCandidate->next.load()))
                {
                    this->head.compare_exchange_strong(localHead, popCandidate);
                    continue;
                }
                
                ret = popCandidate->val;
                Node* candidateNext = popCandidate->next.load();
                if(popCandidate->next.compare_exchange_strong(candidateNext, setMark(candidateNext)))
                {
                    this->head.compare_exchange_strong(localHead, popCandidate);
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

};

void insertTestA(QueueInterface &q)
{
    int r;
    for(int i = 0 ; i < TIMES ; i++)
    {
        if (i % 5 == 0)
        {
            q.pop(r);
        }
        else
        {
            q.push(i);
        }
    }
}

void insertTestB(QueueInterface &q)
{
    for(int i = 0 ; i < TIMES ; i++)
    {
        q.push(i);
    }
}

// For moniter head and tail concurrently
void insertTestC(QueueInterface &q)
{
    while(true)
    {
        cout << "MONITOR: " << q.head << ' ' << q.tail << endl;
        std::this_thread::sleep_for(0.1s);
    }
}



int main()
{
    // Already use polymorphism
    // Just Change the Type "MSqueue" or "AwesomeQueue"
    AwesomeQueue q;
    vector<thread> workers;
    auto start = std::chrono::steady_clock::now();
    for(int i = 0 ; i < NUM_THREAD ; i++)
    {
        workers.push_back(std::move(thread{insertTestA, std::ref(q)}));
    }

    
   
   for(auto &t : workers)
   {
       t.join();
   }


    auto end = std::chrono::steady_clock::now();
    cout << "Total Time: "  << std::chrono::duration_cast<chrono::microseconds>(end - start).count()/1000.0 << " ms" << endl;


    return 0;
}