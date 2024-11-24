#!/bin/bash

docker compose up --build -d
sleep 10

docker compose logs

docker compose down