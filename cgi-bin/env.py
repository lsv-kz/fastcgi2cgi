#!/usr/bin/python3

import os, datetime, sys
import codecs

print("Content-Type: text/html\n\n")
print("""<!DOCTYPE HTML>
<html>
<head>
<meta charset='utf-8'>
<title>Тест</title>
</head>
<body>
<center>
<table border=1>
""")

list_env = os.environ
for i in list_env:
    print('<tr><td>{}</td><td>{}</td></tr>'.format(i, list_env[i]))

now = datetime.datetime.utcnow()
print("""</table>
<form action=\"env.py\" method=\"%s\">
<input type=\"hidden\" name=\"par1\" value=\".-./. .+.!.?.,.~.#.&.>.<.^.\">
<p><input type=\"submit\" value=\"Get ENV\"></p>
</form>
</center>
<hr>
%s
</body>
</html>""" %(os.environ['REQUEST_METHOD'], now.strftime("%a, %d %b %Y %H:%M:%S GMT")))
