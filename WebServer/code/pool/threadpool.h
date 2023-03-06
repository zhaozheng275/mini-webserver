#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
class ThreadPool {
public:
    //expicit不会发生隐式转换
    explicit ThreadPool(size_t threadCount = 4): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);
            for(size_t i = 0; i < threadCount; i++) {
                std::thread([pool = pool_] {    
                    std::unique_lock<std::mutex> locker(pool->mtx); //定义锁
                    while(true) {
                        if(!pool->tasks.empty()) {      //任务队列不为空
                            auto task = std::move(pool->tasks.front()); //移动拷贝得到任务函数
                            pool->tasks.pop();  //尾删
                            locker.unlock();    //解锁
                            task();     //运行
                            locker.lock();  //上锁
                        } 
                        else if(pool->isClosed) break;  //关闭
                        else pool->cond.wait(locker);   //任务队列为空，阻塞
                    }
                }).detach();    //线程分离
            }
    }

    ThreadPool() = default;     //默认构造

    ThreadPool(ThreadPool&&) = default;     //默认拷贝构造
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx); //上锁
                pool_->isClosed = true;     //线程关闭标志
            }
            pool_->cond.notify_all();   //唤醒所有线程使其关闭
        }
    }

    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);     //上锁
            pool_->tasks.emplace(std::forward<F>(task));    //把任务加到任务队列
        }
        pool_->cond.notify_one();   //唤醒一个
    }

private:
    //线程池类
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;
    };
    //线程池的共享指针
    std::shared_ptr<Pool> pool_;
};


#endif //THREADPOOL_H