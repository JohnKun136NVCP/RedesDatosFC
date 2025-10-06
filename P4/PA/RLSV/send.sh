#!/bin/bash
SERVER=$1
FILES=(*.txt) # Puedes cambiar la extencion
NUM_FILES=${#FILES[@]}
INDEX=$((RANDOM % NUM_FILES))
FILE=${FILES[$INDEX]}
echo "Sending $FILE to $SERVER"
./client "$FILE" $SERVER