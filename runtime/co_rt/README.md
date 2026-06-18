`libco`是一个轻量级协程runtime，后续将运行在BlockISA轻核子系统中做用户态线程的管理。


## 协程的context
协程在运行的过程中可以自主让出控制权，进而切换至其他协程。这个过程中就需要对协程的context做保存和恢复，协程的context包括：
* 栈指针
* 父节点（指向父协程的指针）
* env_buf (所有的callee-saved寄存器，利用setjmp实现)
* 函数入口
* 函数参数

当前libco的实现中，`struct co`就是协程的context。

## 函数调用栈stack的管理
* 每一个函数运行在自己独立的调用栈上，栈作为协程context的一部分会被保存和恢复。
* 在创建新协程的时候，新协程会运行在新分配出的栈空间上
* 当协程结束退出之前，协程将一直运行在自己的栈空间上，直至运行结束切换至其他协程
* 函数调用栈的分配可以在初始化的时候静态分配，也可以动态按需分配
* libco给出了X86和ARM版本的协程切换函数调用栈的实现，BlockISA版本待实现

![](./stack_layout.png)

## libco接口介绍
`co_start`：注册新协程的信息，包括函数入口和参数信息，同时进行栈分配和其他初始化工作。注意：新的协程并不会马上执行，这里仅仅是完成新协程的注册。
`co_yield`：协程让出执行权，保存上下文后切换其他协程。libco的实现是通过setjmp/longjmp完成状态的保存和恢复。
`co_wait`：等待child coroutine执行结束，并将已结束的coroutine资源进行释放。

``` c
struct co* co_start(const char *name, void (*func)(void *), void *arg);
void co_yield();
void co_wait(struct co *co);
```

## 当前状态
* 内存的管理当前使用的是malloc/free接口，后续将会替换成内存管理HAC接口。
* 由于协程管理的硬件单元(MP)尚未实现，当前libco runtime是使用软件链表做实现。后续当硬件管理单元(MP)确定并建模完成，会将链表的相关实现做替换。

![](./NP_FSM.png)


## 测试
`./tests`目录下有两个测试用例，使用方法是执行`make test`命令。
