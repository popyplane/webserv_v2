#!/usr/bin/env python3
import sys

# Minimal CGI headers
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("Hello from CGI!\r\n")
sys.stdout.flush()