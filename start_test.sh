#rm test1.log
#nohup ./build/fftrans --inputfile=../wkc.264 --devid_start=0 --dev_num=9  --num=162 --max_frame=-1 >test1.log 2>&1 &
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=1920 --height=1080 --num=1
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=1280 --height=720 --num=4
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=854  --height=480 --num=8
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=640 --height=360 --num=12
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=426 --height=240 --num=18
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=352 --height=288 --num=18
# D1 
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=704 --height=576 --num=10
./build/fftrans --inputfile=/home/yuan/wkc.264 --devid_start=0 --dev_num=1  --width=720 --height=576 --num=10


