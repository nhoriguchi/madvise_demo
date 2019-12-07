# MADV_DONTNEED	 4
# MADV_FREE		 8
# MADV_REMOVE	 9
# MADV_COLD	    20
# MADV_PAGEOUT  21

make || exit 1

TMPD=/tmp/madv_res
rm -rf $TMPD/
mkdir -p $TMPD

echo never > /sys/kernel/mm/transparent_hugepage/enabled

for map in 0 1 2 3 ; do
	for madv in 4 8 9 20 21 ; do
		mkdir -p $TMPD/${map}_${madv}
		./sample2 $map $madv > $TMPD/${map}_${madv}/res
		mv /tmp/smaps* $TMPD/${map}_${madv}/
	done
done
