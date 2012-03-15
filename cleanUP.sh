#!/bin/sh

ps -e | grep router | awk '{print $1;}'| xargs kill -9
ps -e | grep dl_server | awk '{print $1;}'| xargs kill -9
ps -e | grep dl_client | awk '{print $1;}'| xargs kill -9

