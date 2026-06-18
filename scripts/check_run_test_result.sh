#!/bin/bash
cd /home/blockisa/5g_build/5g_Main/BlockISA/test/SuperTest/
if [ ! -d "/home/blockisa/5g_build/5g_Main/BlockISA/test/SuperTest/report_ca/" ];then
  echo "test未生成报告"
  exit 1
  else
  echo "test已生成报告"
fi
cd /home/blockisa/5g_build/5g_Main/BlockISA/test/SuperTest/report_ca/
cat summary.txt 
runresult=$(egrep -E -n 'Failed 0' summary.txt)
# runresult = "Failed 0"
echo $runresult
if [[ ! -z $runresult ]];
then
  echo "Failed test equal to 0"
else
  echo "Failed test not equal to 0"
  exit 1
fi
