#!/usr/bin/env python3

import os, datetime, sys, urllib.parse

try:
    meth = os.environ['REQUEST_METHOD']
except:
    meth = 'None'

print("Content-Type: text/html")
print("Pragma: no-cache")
print("");

print("""<!DOCTYPE HTML>
<html>
<head>
<meta charset='utf-8'>
<title>Тест</title>
</head>
<body>
""")

list_env = os.environ
for i in list_env:
    print('<b>{}: {}</b><br>'.format(i, list_env[i]))

if meth == 'POST':
    post_data = urllib.parse.parse_qsl(sys.stdin.read())
    print('<p>stdin: {}</p>'.format(post_data))
    for i in post_data:
        print('<b>{}: {}</b><br>'.format(i[0], i[1]))
elif meth == 'GET':
    get_data = urllib.parse.parse_qsl(os.environ['QUERY_STRING'])
    print('<p>{}</p>'.format(get_data))
    for i in get_data:
        print('<b>{}: {}</b><br>'.format(i[0], i[1]))

now = datetime.datetime.utcnow()
print("""<form action=\"env.py\" method=\"%s\">
<input type=\"hidden\" name=\"par1\" value=\".-./. .+.!.?.,.~.#.&.>.<.^.\">
<p><input type=\"submit\" value=\"Get ENV\"></p>
</form>
<hr>
%s
</body>
</html>""" %(meth, now.strftime("%a, %d %b %Y %H:%M:%S GMT")))
