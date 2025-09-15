#!/bin/bash
SERVER=$1
PORT=$2
FILES=(*.txt) 
NUM_FILES=${#FILES[@]}
INDEX=$((RANDOM % NUM_FILES))
FILE=${FILES[$INDEX]}
echo "Sending $FILE to $SERVER:$PORT"
./c "$FILE" $SERVER

