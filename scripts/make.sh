#!/bin/bash

#(c) 2008 by Malte Marwedel. don't ask how its done ;-)
#its just for compiling the scripts pack.sh and unpack.sh together

#unfortunately 'replace' is only available if mysql server is installed :-(
#replace \" \\\" \` \\\` \$ \\\$ < unpack.sh > temp_enc
#MAGIC=`cat temp_enc`
#rm -f temp_enc
#it was faster too, so now the bash is used, based on
#http://tldp.org/LDP/abs/html/string-manipulation.html

#how to write functions: http://tldp.org/LDP/abs/html/complexfunct.html

encapsulate () {
  RETVAL=${1//\"/\\\"} #"} #uses the first argument as input
  RETVAL=${RETVAL//\`/\\\`}
  RETVAL=${RETVAL//\$/\\\$}
}


UNPACK=`cat unpack.sh`

encapsulate "$UNPACK"
UNPACK="$RETVAL"

#replace MAGICMARK "$MAGIC" <pack.sh > cb_make_selfextracting.sh

PACK=`cat pack.sh`

PACKUNPACK=${PACK/__MAGICMARK__/"$UNPACK"}

echo "$PACKUNPACK" > cb_make_selfextracting.sh

chmod u+x cb_make_selfextracting.sh

#and the same for the example xml file for the automatic script

#replace \" \\\" \` \\\` \$ \\\$ < emptydemo.xml > temp_enc
#MAGIC=`cat temp_enc`
#rm -f temp_enc

SAMPLE=`cat emptydemo.xml`

encapsulate "$SAMPLE"
SAMPLE="$RETVAL"

#replace __EMPTYDEMO__ "$MAGIC" <cb_automatic.sh > cb_automatic_temp.sh

AUTOMATIC=`cat cb_automatic.sh`
AUTOMATIC=${AUTOMATIC/__EMPTYDEMO__/"$SAMPLE"}
echo "$AUTOMATIC" > cb_automatic_temp.sh

chmod u+x cb_automatic_temp.sh

