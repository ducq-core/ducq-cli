gcc ../main.c generic_client.c -lducq -llua -lm -o ducq.out
mv ducq.out ~/.local/bin/ducq

gcc ../main.c monitor_client.c -lducq -llua -lm -o monitor.out
mv monitor.out ~/.local/bin/monitor

