#!/usr/bin/python3

import os

# Standard CGI headers
print("Content-Type: text/html") # Or text/plain
print("Status: 200 OK")
print() # Essential blank line separating headers from body

# HTML body
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Python CGI Test</title></head>")
print("<body>")
print("<h1>Hello from Python CGI!</h1>")
print("<p>Current working directory: " + os.getcwd() + "</p>")
print("<p>Environment variables:</p>")
print("<ul>")
for key, value in os.environ.items():
    print(f"<li><strong>{key}</strong>: {value}</li>")
print("</ul>")
print("</body>")
print("</html>")