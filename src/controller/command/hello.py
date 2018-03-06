#!/usr/bin/env python
# coding: utf-8


class Hello(object):
    def __init__(self, name):
        self.name = name

    def say(self):
        print("Hello, %s!" % self.name)


def do_hello(name):
    """Say hello to given user

    spp > hello alice
    Hello, alice!
    """

    if name == '':
        print('name is required!')
    else:
        hl = Hello(name)
        hl.say()

if __name__ == "__main__":
    hello = Hello()
    print(hello.say())
