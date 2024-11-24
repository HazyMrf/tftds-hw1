#!/bin/bash

docker compose up --build -d

docker compose stop worker2
sleep 20

docker compose logs

docker compose down 