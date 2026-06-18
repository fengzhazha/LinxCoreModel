1. cd BlockISA/bin
2. 生成konata标准格式log,需要设置-t参数为4并使用-o参数指定文件输出路径

```
lightcore -t 4 -f [测试用例] -o [konata日志输出路径]
例: lightcore -t 4 -f ../../SuperTest_BlockISA/run_log/suite/3/6/5/tdoeke2.c/tdoeke2 -o ../konata.log
```
3. 将生成的konata日志使用konata软件打开。

##### 脚本二次处理相关其他功能
脚本位置：BlockISA/scripts/konata/konatalogmanager.py

包括以下功能:

功能一：自动给没有正常retire的指令添加retire log，结束其生命周期，防止konata软件因为渲染问题卡死

使用方式：
1. cd BlockISA/scripts/konata
2. 运行脚本
可以不指定输出路径运行：

```
python konatalogmanager.py 1 [konata log文件路径] 
例：python konatalogmanager.py 1 ../../konata.log
```
参数1为功能序列号,此时输出处理后的文件名称为konata_out.log,放在和输入文件的同级目录。
指定输出路径运行:

```
python konatalogmanager.py 1 [konata log文件路径] [输出文件路径]
例：python konatalogmanager.py 1 ../../konata.log ../../konata_out.log
```
此时输出处理后的文件为配置的路径。

功能二：将konata log按照线程进行拆分,按照单个线程进行显示。

使用方式：
1. cd BlockISA/scripts/konata
2. 运行脚本
   可以不指定输出路径运行：

```
python konatalogmanager.py 2 [konata log文件路径] 
例：python konatalogmanager.py 2 ../../konata.log
```
参数2为功能序列号,此时输出处理后的文件名称为konata_out_{tid}.log,放在和输入文件的同级目录。