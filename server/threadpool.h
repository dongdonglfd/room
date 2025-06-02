//宏定义防止头文件重复包含
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    //构造函数，size_t是创建的线程数
    ThreadPool(size_t);
    //任务队列
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;//尾置限定符，利用result_of来
        //推断调用F后返回值类型，并存储在future中
        //注意：C++17 中 std::result_of 被弃用，推荐使用 std::invoke_result 替代
    //析构函数
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;
    
    // synchronization //同步
    std::mutex queue_mutex;//互斥锁
    std::condition_variable condition;//条件变量
    bool stop;//停止标志
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)// 构造函数，接收一个参数表示线程池中要创建的线程数量
// 每个线程都会在后台等待并处理任务队列的任务
    :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(//类内直接构造//可以传递任何类型的参数
            //捕获外界参数，以便在线程中访问成员变量
            [this]    
            {
                for(;;)//这里就是定义了一个循环的工作线程，检测任务队列是否为空，以及stop
                {
                    std::function<void()> task;//任务

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        //等待h唤醒，若不唤醒，则释放锁阻塞在这里并休眠
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); });//检测，防止虚假唤醒
                        //若stop为停止且任务队列为空，则线程退出
                        if(this->stop && this->tasks.empty())
                            return;
                        //接受任务
                        task = std::move(this->tasks.front());
                        //弹出任务
                        this->tasks.pop();
                    }
                    //执行任务
                    task();
                }
            }
        );
}

// add new work item to the pool
template<class F, class... Args>//定义一个变长模板，F是可调用对象，如函数lambda,functor,Args是参数
auto ThreadPool::enqueue(F&& f, Args&&... args) //使用尾置限定符和future获取返回值，f是可调用对象，如函数lambda,functor,args是参数
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    //简化书写，推导返回值类型
    using return_type = typename std::result_of<F(Args...)>::type;
    //使用packageed_task封装函数，设置return_type类型为返回值
    //再使用make_shared创建一个shared_ptr的智能指针来方便管理对象
    //最后使用bind将可执行函数F和参数args绑定到一起，生成新的可调用对象
    //forward完美转发，与参数的万能引用一起使用，提高效率
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
    //获取task的future
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        //将任务提交到任务队列中
        tasks.emplace([task](){ (*task)(); });
    }
    //唤醒工作线程
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;//设置停止标志
    }
    condition.notify_all();//唤醒所有线程
    //等待所有线程执行完毕并回收资源
    for(std::thread &worker: workers)
        worker.join();
}
#endif