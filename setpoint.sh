#!/bin/bash
echo -ne "\x$1\x$2" | nc 127.0.0.1 3333
