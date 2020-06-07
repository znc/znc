#!/usr/bin/env python

import json
import sys

array = []

data = json.load(sys.stdin)
for user in data['data']:
    user = user['data']
    if user['fullName']:
        array.append('* {} ({})'.format(user['username'], user['fullName']))
    else:
        array.append('* ' + user['username'])

array.sort(key=lambda x: x.lower())

sys.stdout = open('TRANSLATORS.md', 'wt')

print('These people helped translating ZNC to various languages:')
print()
for u in array:
    print(u)
print()
print('Generated from https://crowdin.com/project/znc-bouncer')
