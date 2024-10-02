
gnome-terminal -- /bin/sh -c "/bin/python3 /home/user/IoT_Aulas_Praticas/PROJECT/cloud_bridge.py; exec bash"
gnome-terminal -- /bin/sh -c "/bin/python3 /home/user/IoT_Aulas_Praticas/PROJECT/bus_node_34.py; exec bash"
gnome-terminal -- /bin/sh -c "/bin/python3 /home/user/IoT_Aulas_Praticas/PROJECT/bus_node_24T.py; exec bash"
gnome-terminal -- /bin/sh -c "/bin/python3 /home/user/IoT_Aulas_Praticas/PROJECT/iot-dashboard/app.py"
gnome-terminal -- /bin/sh -c "sudo ~/contiki/tools/tunslip6 -v2 -s /dev/ttyUSB0 fd00::1/64; exec bash"