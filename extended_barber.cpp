#include <thread>
#include <iostream>
#include <string>
#include <chrono>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <pthread.h>

class Semaphore {
public:
    Semaphore (int count_)
        : count(count_) {}

    inline void signal()
    {
        std::unique_lock<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }

    inline void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);

        while(count == 0){
            cv.wait(lock);
        }
        count--;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
};

/*
    Note: barber_B represents the two barbers who can only make haircuts.
    while barber_A represents the barber who can both make haircuts and perm.
 */
Semaphore male_cnt_sig(1), female_cnt_sig(1);        //protects the critical var named male_cnt and female_cnt
Semaphore barber_B_cnt_sig(1);                    //protects the critical var named barber_B_cnt;
Semaphore wakeup_B(0), wakeup_A(0);                  //signaled by the customers to call for the barber's service
Semaphore barber_B_ready(0), barber_A_ready(0);      //signaled by the barbers to call the customer come and offer service for him/her.
//variables in critical section
int male_cnt = 0, female_cnt = 0, barber_B_cnt = 2;

static void* male_customers(void *args);
static void* female_customers(void *args);
static void* barber_A(void * args);
static void* barber_B(void *args);
void enter(int tid, int male_cnt, int female_cnt);
void leave(int tid, int flag);                            //threads end at here
void have_haircut(int tid);
void get_permed(int tid);
void cuthair(int tid);
void perm(int tid);

int main(int argc, char *argv[])
{
    printf("Welcome to the advanced barber shop.\nThe barbers has started another new day.\n\n\n");

    pthread_t A_barber, B_barber[2];
    pthread_create(&A_barber, NULL, barber_A, NULL);
    for(int i = 0; i < 2; i++)
        pthread_create(&B_barber[i], NULL, barber_B, NULL);
    
    pthread_t temp_customers;
    int cnt = 0;
    srand(time(NULL));
    while (cnt < 200)                                           //barbers ought to have rest
    {
        if(rand() % 5 == 0)                                     //The number of gentlemen is 4 times the size of ladies.
            pthread_create(&temp_customers, NULL, female_customers, NULL);
        else
            pthread_create(&temp_customers, NULL, male_customers, NULL);
        pthread_detach(temp_customers);
        cnt++;
        sleep(rand()%3);
    }
    sleep(60);
    printf("\n\nThe day gets dark, no new customers come, press CTRL-C to let the barbers go off work.\n");
    
    return 0;
}

static void* male_customers(void * args)
{
    male_cnt_sig.wait();    female_cnt_sig.wait();
    if(male_cnt + female_cnt < 6)
    {
        male_cnt++;
        enter((int)syscall(SYS_gettid), male_cnt, female_cnt);
    }
    else if(male_cnt + female_cnt < 12)
    {
        if(rand() % 2 == 0)
        {
            male_cnt++;
            enter((int)syscall(SYS_gettid), male_cnt, female_cnt);
        }
        else
        {
            female_cnt_sig.signal();    male_cnt_sig.signal();
            leave((int)syscall(SYS_gettid), 0);
            return NULL;
        }
    }
    else
    {
        female_cnt_sig.signal();    male_cnt_sig.signal();
        leave((int)syscall(SYS_gettid), 0);
        return NULL;
    }
    female_cnt_sig.signal();    male_cnt_sig.signal();

    barber_B_cnt_sig.wait();
    if(barber_B_cnt == 0)
        wakeup_A.signal();
    barber_B_cnt_sig.signal();
    wakeup_B.signal();
    barber_B_ready.wait();
    have_haircut((int)syscall(SYS_gettid));
    leave((int)syscall(SYS_gettid), 1);
    return NULL;
}

static void* female_customers(void * args)
{
    male_cnt_sig.wait();    female_cnt_sig.wait();
    if(male_cnt + female_cnt < 6)
    {
        female_cnt++;
        enter((int)syscall(SYS_gettid), male_cnt, female_cnt);
    }
    else if(male_cnt + female_cnt < 12)
    {
        if(rand() % 2 == 0)
        {
            female_cnt++;
            enter((int)syscall(SYS_gettid), male_cnt, female_cnt);
        }
        else
        {
            female_cnt_sig.signal();    male_cnt_sig.signal();
            leave((int)syscall(SYS_gettid), 0);
            return NULL;
        }
    }
    else
    {
        female_cnt_sig.signal();    male_cnt_sig.signal();
        leave((int)syscall(SYS_gettid), 0);
        return NULL;
    }
    female_cnt_sig.signal();    male_cnt_sig.signal();

    wakeup_A.signal();
    barber_A_ready.wait();
    get_permed((int)syscall(SYS_gettid));
    leave((int)syscall(SYS_gettid), 1);
    return NULL;
}

static void* barber_B(void * args)
{
    while (true)
    {
        wakeup_B.wait();
        male_cnt_sig.wait();
        if(male_cnt == 0)
            male_cnt_sig.signal();
        else
        {
            male_cnt--;
            male_cnt_sig.signal();
            barber_B_cnt_sig.wait();
            barber_B_cnt--;
            barber_B_cnt_sig.signal();
            barber_B_ready.signal();
            cuthair((int)syscall(SYS_gettid));
            barber_B_cnt_sig.wait();
            barber_B_cnt++;
            barber_B_cnt_sig.signal();
        }
    }
    return NULL;
}

static void* barber_A(void * args)
{
    while (true)
    {
        wakeup_A.wait();
        female_cnt_sig.wait();
        if(female_cnt > 0)
        {
            female_cnt--;
            barber_A_ready.signal();
            female_cnt_sig.signal();
            perm((int)syscall(SYS_gettid));
        }
        else
        {
            female_cnt_sig.signal();
            male_cnt_sig.wait();
            if(male_cnt == 0)
                male_cnt_sig.signal();
            else
            {
                barber_B_cnt_sig.wait();
                if(barber_B_cnt != 0)
                    barber_B_cnt_sig.signal();
                else
                {
                    barber_B_cnt_sig.signal();
                    male_cnt--;
                    male_cnt_sig.signal();
                    barber_B_ready.signal();
                    cuthair((int)syscall(SYS_gettid));
                }
            }
        }
    }
    return NULL;
}

void enter(int tid, int male_cnt, int female_cnt)
{
    printf("Customer thread %d enters the barber shop. Now there are %d ladies and %d gentlemen waiting.\n", tid, female_cnt, male_cnt);
}

void leave(int tid, int manner)
{
    if(manner == 0)
        printf("Customer thread %d feels the shop too crowded and left.\n", tid);
    else
        printf("Customer thread %d left the shop with his service finished.\n", tid);    
}

void have_haircut(int tid)
{
    printf("Customer thread %d is having a haircut and it lasts for 1 second.\n", tid);
    sleep(1);
}

void cuthair(int tid)
{
    printf("Barber thread %d is hairdressing his customer and it lasts for 1 second.\n", tid);
    sleep(1);
}

void get_permed(int tid)
{
    printf("Customer thread %d is getting herself permed and it lasts for 6 seconds.\n", tid);
    sleep(6);
}

void perm(int tid)
{
    printf("Barber thread %d is perming his customer and it lasts for 6 seconds.\n", tid);
    sleep(6);
}
