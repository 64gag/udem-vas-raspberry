echo "Run this program with any option to restore interfaces file."
echo "Reboot after running this script."

if [ $# -eq 0 ]
  then
    echo "Copying the ad-hoc interfaces file..."

	if [ -f /etc/network/interfacesold ];
	then
		echo "File interfacesold exists, not backing it up again."
	else
		echo "Backing up interfaces file."
		sudo mv /etc/network/interfaces /etc/network/interfacesold
	fi
	sudo cp ./interfaces /etc/network/interfaces
	echo "Done!"
else
	echo "Restoring interfacesold file..."
	if [ -f /etc/network/interfacesold ];
	then
		sudo mv /etc/network/interfacesold /etc/network/interfaces
		echo "Done!"
	else
		echo "No interfacesold file to restore!"
	fi
fi
