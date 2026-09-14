import socket
import os
import time
import subprocess

def is_connected(host="8.8.8.8", port=53, timeout=3):
    """
    Host: 8.8.8.8 (google-public-dns-a.google.com)
    OpenPort: 53/tcp
    Service: domain (DNS/TCP)
    """
    try:
        socket.setdefaulttimeout(timeout)
        socket.socket(socket.AF_INET, socket.SOCK_STREAM).connect((host, port))
        return True
    except socket.error as ex:
        print(ex)
        return False


def reconnect_wifi_windows(my_interface_name="Wi-Fi", my_router_name="ROUTERNAME"):
    """
    Disable and re-enable the Wi-Fi interface on Windows to force a reconnect.
    Requires running the script as an administrator.
    """
    print(f"Attempting to reconnect '{my_interface_name}' to '{my_router_name}'...")
    try:
        # Disable the interface
        os.system(f'netsh interface set interface "{my_interface_name}" disable')
        time.sleep(3) # Wait for the interface to disable

        # Enable the interface
        os.system(f'netsh interface set interface "{my_interface_name}" enable')
        time.sleep(5) # Wait for the interface to enable

        # Connect to a specific SSID (if not automatic)
        # Note: A profile for this network must already exist on the system.
        os.system(f'netsh wlan connect name="{my_router_name}"')
        time.sleep(5) # Wait for connection attempt to complete

        print("Reconnection commands issued. Checking connectivity...")

    except Exception as e:
        print(f"An error occurred during reconnection: {e}")


while True:
    if is_connected():
        print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Connected to the internet. Monitoring again in 60 seconds.")
    else:
        print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Not connected to the internet. Attempting to reconnect...")
        # Replace with your specific Wi-Fi interface name and SSID
        reconnect_wifi_windows(interface_name="{my_interface_name}", router_name="my_router_name")
        # Add a longer sleep after a failed connection/reconnection attempt
        time.sleep(10) 
        
    time.sleep(60)
