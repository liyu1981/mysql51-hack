#!/bin/sh
SHM_NUM=8
OP=list
let key=0
function ftok() {
	pathname=$1;
	proj_id=$2;

	str_st_ino=`stat --format='%i' "${pathname}" 2>/dev/null`;
	str_st_dev=`stat --format='%d' "${pathname}" 2>/dev/null`;
	if [ "x${str_st_ino}" = "x" -o "x${str_st_dev}" = "x" ] ; then
		return 1;
	fi

	let st_ino=${str_st_ino}
	let st_dev=${str_st_dev}

	# 注意这里的位操作运算符需要加转义符
	let key1=${st_ino}\&16#FFFF
	let key2=${st_dev}\&16#FF
	let key2=${key2}\<\<16
	let key3=${proj_id}\&16#FF
	let key3=${key3}\<\<24
	let key=${key1}\|${key2}
	let key=${key}\|${key3}
}

function echohelp(){
	echo "ftok generator"
	echo "Usage:$1 [list|rm] pathname"
	exit 5
}

function rm_shm(){
    id=1
    while [ $id -le $SHM_NUM ]; do
        ftok $sPathName $id
        printf "%s, %d, %#x\n" $sPathName $id $key
        ipcrm -M $key
        id=`expr $id + 1`
    done;
}


function list_shm(){
    id=1
    while [ $id -le $SHM_NUM ]; do
        ftok $sPathName $id
        printf "%s, %d, %#x\n" $sPathName $id $key
        #ipcrm -M $key
        id=`expr $id + 1`
    done;
}

if [ $# -ne 2 ] ; then
	echohelp $0
fi

sPathName=$2
OP=$1

if [ "${sPathName:0:1}" != "/" ] ; then
	sPathName=${PWD}/${sPathName}
fi

if ! test -d ${sPathName} ; then
	echo "No File Found![${sPathName}]"
	exit 4
fi

if [ $OP="list" ]; then
    list_shm
elif [ $OP="rm" ]; then
    rm_shm
else
    list_shm
    #echo "$OP is invalid."
fi
#ftok "${sPathName}" "${nProjectID}"
#echo ${key}
#printf "%#x\n" $key
