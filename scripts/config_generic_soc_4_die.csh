#! /bin/csh

rm -f  parameter_base  parameter_1  parameter_IO parameter_IO_2  parameter_IO_3
if (! -e "parameter_base") then
    ln -s model/generic_soc/export_generic_mp_generic/parameter_base  parameter_base
endif

if (! -e "parameter_1") then
    ln -s model/generic_soc/export_generic_mp_generic/parameter_1     parameter_1
endif

if (! -e "parameter_IO") then
    ln -s model/generic_soc/export_generic_mp_generic/parameter_IO     parameter_IO
endif

if (! -e "parameter_IO_2") then
    ln -s model/generic_soc/export_generic_mp_generic/parameter_IO     parameter_IO_2
endif

if (! -e "parameter_IO_3") then
    ln -s model/generic_soc/export_generic_mp_generic/parameter_IO     parameter_IO_3
endif

cp -L -r ./parameter_base/daw_chip.cfg ./parameter_base/daw.cfg
cp -L -r ./parameter_base/daw_chip.cfg ./parameter_0/daw.cfg
cp -L -r ./parameter_base/daw_chip.cfg ./parameter_1/daw.cfg
sed -i 's/number_of_die.*/number_of_die       4/' export_generic_mp_generic/soc_md.cfg
sed -i 's/die_type.*/die_type            0 0 1 1/' export_generic_mp_generic/soc_md.cfg
sed -i 's/#l3_tag_port_list        0x50 0x51/l3_tag_port_list        0x50 0x51/' parameter_1/TopologyAndIDMap.cfg
sed -i 's/#l3_tag_port_list        0x50 0x51/l3_tag_port_list        0x50 0x51/' parameter_base/TopologyAndIDMap.cfg
cp -f parameter_0/manyring.csv parameter_1/manyring.csv
if (! -e "log/ub_log") then
    mkdir log/ub_log
endif





