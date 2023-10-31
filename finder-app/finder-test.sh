#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR="AELD_IS_FUN"
WRITEDIR="/tmp/aeld-data"
#username=$(cat conf/username.txt)
username=$(cat /etc/finder-app/conf/username.txt)
if [ $# -lt 3 ]
then
	echo "Using default value '${WRITESTR}' for string to write"
	if [ $# -lt 1 ]
	then
		echo "Using default value ${NUMFILES} for the number of files to write"
	else
		NUMFILES=$1
	fi	
else
	NUMFILES=$1
	WRITESTR=$2
	WRITEDIR="/tmp/aeld-data/$3"
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string '${WRITESTR}' to '${WRITEDIR}'"

rm -rf "${WRITEDIR}"

# create $WRITEDIR if not assignment1
#assignment=$(cat conf/assignment.txt)
#assignment=$(cat etc/finder-app/conf/assignment.txt)
assignment=`cat /etc/finder-app/conf/assignment.txt`
if [ "$assignment" != 'assignment1' ]
then
	mkdir -p "$WRITEDIR"

	if [ -d "$WRITEDIR" ]
	then
		echo "$WRITEDIR created"
	else
		exit 1
	fi
fi

# Compile the writer application
##make
 
for i in $( seq 1 $NUMFILES)
do
        ##WRITEDIR=${WRITEDIR#/}
	##./writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
	writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

current_directory=$(pwd)
echo "Current working directory is: $current_directory"

files=$(ls)
echo "files in directory is: $files"

#chmod +x "finder.sh"

#OUTPUTSTRING=$(. "./finder.sh" "$WRITEDIR" "$WRITESTR")
OUTPUTSTRING=$(${current_directory}/finder.sh "$WRITEDIR" "$WRITESTR")
echo "Output from finder program: $OUTPUTSTRING"

# Remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
        echo ${OUTPUTSTRING} >/tmp/assignment4-result.txt
	echo "Success"
	exit 0
else
        echo ${OUTPUTSTRING} >/tmp/assignment4-result.txt
	echo "Failed: Expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
	exit 1
fi

