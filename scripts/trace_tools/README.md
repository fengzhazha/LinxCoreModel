`trace_analyze.py`是一个trace和反汇编的分析脚本，使用方式如下：

方式1：
``` shell
python trace_analyze.py 1 <obj.log> <bpc.log>
```
根据`bpc.log`文件，打印出所有微指令的trace。

方式2：
``` shell
python trace_analyze.py 2 <symtable.log> <bpc.log>
```
根据`bpc.log`文件，打印出所有每条bpc指令所对应的函数名信息。


输入文件格式介绍：
* **obj.log**: 二进制ELF的反汇编文件，生成方式`xxx-objdump -d <ELF.bin>`。
* **symtable.log**： 二进制ELF的符号信息文件，生成方式`xxx-readelf -s <ELF.bin>`。
* **bpc.log**： 由QEMU产生的bpc trace log。当前脚本仅接受只有bpc信息的trace，格式参见本目录`test_bpc.log`。


**注**：当前脚本没有添加对输入文件的格式检查，请自行保证文件的格式如示例所示。