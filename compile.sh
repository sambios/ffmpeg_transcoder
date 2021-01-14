
btype="bmcodec"
if [ "$1" ];then
     btype=$1
fi

echo build_type=$btype

rm -fr build
mkdir build
cd build

if [ $btype == "bmcodec" ];then
    echo "not dump file"
    cmake -DCMAKE_BUILD_TYPE=Release -DUSE_BMCODEC=ON -DUSE_DEV=OFF -DUSE_FFMPEG_STATIC=ON ..
elif [ $btype == "emu" ]; then
    echo "based on software ffmpeg"
    cmake -DUSE_OUTPUT_FILE=OFF -DUSE_BMCODEC=OFF -DUSE_DEV=OFF -DUSE_FFMPEG_STATIC=OFF ..
else
    echo unsupport btype=$btype
    exit 1
fi

make -j4
cd ..

