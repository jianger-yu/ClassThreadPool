#include <pthread.h>
#include <iostream>
#include <stdlib.h>
#include <fcntl.h>
#include <error.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <string>

int ret = 0;

void sys_err(int ret, std::string str){
    if (ret != 0){
        std::cout << str;
        fprintf(stderr, " error:%s\n", strerror(ret));
        exit(1);
    }
}

// 任务队列（链表）类
class task{
public:
    void *(*function)(void *); // 任务处理函数
    void *arg;                 // 任务参数
    struct task *next;         // 链表实现
};

// 线程池类
class pthread_pool{
private:
    pthread_mutex_t lock;       // 互斥锁
    pthread_cond_t cond;        // 条件变量
    task *task_queue;           // 任务队列
    pthread_t *pthreads;        // 线程数组
    int pth_cnt;                // 线程数量
    bool stop;                  // 是否运行

//每个线程优先执行此函数
static void* pthreadrun(void* arg) {
    pthread_pool* pool = static_cast<pthread_pool*>(arg);
    pool->consumer(pool);
    return nullptr;
}

public:
    // 消费者
void *consumer(pthread_pool* pool){
    while (1){
        pthread_mutex_lock(&lock); // 上锁

        while (pool->task_queue == NULL && pool->stop == false){
            pthread_cond_wait(&pool->cond, &pool->lock);
        }
        // 出循环，表示已经拿到了锁且有任务
        // 检查是否需要终止线程
        if (pool->stop){
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        // 拿取任务
        task *mytask;
        mytask = pool->task_queue;
        pool->task_queue = pool->task_queue->next;
        // 拿完任务直接解锁
        pthread_mutex_unlock(&pool->lock);

        // 执行任务
        mytask->function(mytask->arg);
        free(mytask);
    }
    return NULL;
}

//构造函数，初始化线程池
pthread_pool(int ThreadCount){
    ret = pthread_mutex_init(&lock, NULL); // 互斥锁
    sys_err(ret, "mutex_init");

    ret = pthread_cond_init(&cond, NULL); // 条件变量
    sys_err(ret, "cond_init");

    task_queue = NULL;                                               // 任务队列
    pthreads = (pthread_t *)malloc(ThreadCount * sizeof(pthread_t)); // 线程数组
    pth_cnt = ThreadCount;                                           // 线程数量
    // 生成线程
    for (int i = 0; i < ThreadCount; i++){
        ret = pthread_create(&pthreads[i], NULL, pthreadrun, this);
        sys_err(ret, "pthread_create");
    }
    stop = false;
}

// 添加任务到线程池
void PushTask(void *(*function)(void *), void *arg){
    task *NewTesk = (task *)malloc(sizeof(task));
    NewTesk->function = function;
    NewTesk->arg = arg;
    pthread_mutex_lock(&this->lock); // 上锁，写入公共空间

    if (this->task_queue == NULL){
        this->task_queue = NewTesk;
    }
    else{
        task *p = this->task_queue;
        while (p->next != NULL)
            p = p->next;
        p->next = NewTesk;
    }

    pthread_cond_signal(&this->cond);  // 唤醒工作线程
    pthread_mutex_unlock(&this->lock); // 解锁
}

//析构函数，释放线程池
~ pthread_pool(){
    pthread_mutex_lock(&this->lock);     // 上锁
    this->stop = true;                   // 准备令全部线程开始停止
    pthread_cond_broadcast(&this->cond); // 叫醒全部线程
    pthread_mutex_unlock(&this->lock);   // 解锁

    // 回收所有线程
    for (int i = 0; i < this->pth_cnt; i++){
        pthread_join(this->pthreads[i], NULL);
    }

    free(this->pthreads); // 释放线程数组

    // 释放任务队列
    task *pt;
    while (this->task_queue){
        pt = this->task_queue;
        this->task_queue = pt->next;
        free(pt);
    }

    // 销毁锁和条件变量
    pthread_mutex_destroy(&this->lock);
    pthread_cond_destroy(&this->cond);
}
};

void *calcu(void *dig){
    int ans = 1, tmp = (long long)dig;
    while (tmp)
        ans *= tmp--;
    printf("%lld 的阶乘为：%d\n", (long long)dig, ans);
    return NULL;
}
void *producter(int n,pthread_pool &pool){
    while (n--){
        long long arg = rand() % 12 + 1;
        pool.PushTask(calcu, (void *)arg);
        usleep(2500);
    }
    return NULL;
}

int main(){
    srand(time(NULL));
    pthread_pool pool(10); // 初始化10个线程

    producter(3000,pool); // 生产任务

    sleep(5); // 等待任务执行

    return 0;
}