from hashlib import md5
from random import choice
from string import ascii_letters, digits

import sys


PREFIX = "flagrom-"

def generate_md5(target_hash):
    while True:
        next_string = PREFIX + ''.join(choice(ascii_letters + digits) for _ in range(10))
        if md5(next_string.encode()).hexdigest().startswith(target_hash):
            print("Found it!")
            print(next_string)
            break
    return next_string