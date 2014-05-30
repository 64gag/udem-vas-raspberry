sudo apt-get update
sudo apt-get upgrade
sudo apt-get install libsdl1.2-dev libsdl-gfx1.2-dev vim git
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.36.tar.gz
tar zxvf bcm2835-1.36.tar.gz
cd bcm2835-1.36
./configure
make
sudo make install
rm ./bcm2835-1.36.tar.gz
rm -r ./bcm2835-1.36
echo "git clone https://github.com/paguiar/udem_vas_raspberry.git" > clone_udem_vas_raspberry.sh
chmod +x ./clone_udem_vas_raspberry.sh
