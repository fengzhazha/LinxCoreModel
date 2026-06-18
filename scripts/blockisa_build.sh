log_info "run blockisa_build.sh"
cd /home/blockisa/5g_build/5g_Main/BlockISA/
if [ ! -d "/build" ];then
  mkdir build
  else
  echo "build文件夹已经存在"
fi
cd /home/blockisa/5g_build/5g_Main/BlockISA/build/
rm -rf *
cmake ../
echo "make开始编译"
make -j 160
echo "make编译成功"