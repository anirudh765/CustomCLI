#!/bin/bash

pkill auto_delete_daemon 2>/dev/null || true

make clean
make
make install

echo "alias rm=\"$HOME/bin/auto_delete delete\"" >> $HOME/.bashrc
echo "alias trash-list=\"$HOME/bin/auto_delete list\"" >> $HOME/.bashrc
echo "alias trash-restore=\"$HOME/bin/auto_delete restore\"" >> $HOME/.bashrc
echo "alias trash-purge=\"$HOME/bin/auto_delete purge\"" >> $HOME/.bashrc

ln -sf $HOME/bin/auto_delete $HOME/bin/trash

if [ -d /etc/systemd/system ]; then
    cat > auto_delete_daemon.service << EOF
[Unit]
Description=Auto Delete Daemon
After=network.target

[Service]
Type=simple
User=$(whoami)
ExecStart=$HOME/bin/auto_delete_daemon
Restart=on-failure
RestartSec=30

[Install]
WantedBy=multi-user.target
EOF

    if command -v sudo &> /dev/null; then
        sudo mv auto_delete_daemon.service /etc/systemd/system/ && \
        sudo systemctl daemon-reload && \
        sudo systemctl enable auto_delete_daemon && \
        sudo systemctl start auto_delete_daemon && \
        echo "Daemon installed and started via systemd"
    else
        echo "Cannot install daemon service (sudo not available)"
    
    fi
fi

echo "AutoDelete system has been installed! Restart your terminal or run 'source $HOME/.bashrc'"