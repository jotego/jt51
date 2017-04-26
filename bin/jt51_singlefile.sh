#!/bin/bash
output_name=jt51_1v1.v
output_file=$jt51/release/$output_name

if [ ! -e "$jt51" ]; then
	echo System variable \$jt51 must be defined to point
	echo to the root folder of jt51
	exit 1
fi

rm -f $output_file

for i in $(cat $jt51/ver/common/basic.f); do
	thisfile=$jt51/hdl/$(basename $i)
	if [ ! -e "$thisfile" ]; then
		echo "Cannot find file $thisfile"
		exit 1
	fi
	echo $(basename $i)
	cat $jt51/hdl/$(basename $i)>>$output_file;
done

echo Single file version of jt51 is ready at $output_file
