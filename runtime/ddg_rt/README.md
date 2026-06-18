<img src="./docs/pics/ddg-pipeline-parallel.png" style="zoom:75%;" />

20241105: 
1. 将DDG的解依赖算法分成了4个pipeline stage，其主要目的是并发图计算中的访存请求。每个stage的工作主要就是节点的处理和访存，
其中第四个stage仅做Task类的初始化，后续可以视情况去除。

2. 相较于之前振东的代码，增加了程序退出的判断，当下述条件全部满足的时候，程序退出：
    * 所有worker都是idle状态
    * 所有的queue都是空

3. 当前Queue的实现基于C++的Concurrent Queue，需要后续移植到GQM接口。