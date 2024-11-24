#!/bin/bash

docker compose up --build -d
sleep 10

docker compose exec worker2 iptables -A INPUT --mode random --probability 1 -j DROP
sleep 15

docker-compose logs

docker-compose down