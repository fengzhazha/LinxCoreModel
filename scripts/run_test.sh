log_info "run run_test.sh"
cd ../test/SuperTest/
if [ ! -d "/suite_dir" ];then
  mkdir suite_dir
  else
  echo "suite_dir文件夹已经存在"
fi
./run_gftb.sh run -j 160 -m /home/blockisa/5g_build/5g_Main/BlockISA/ -t /home/blockisa/blocktest/suite/ -n