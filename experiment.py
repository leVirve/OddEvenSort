#! /usr/bin/python
from tools.submit import Testcase, Scheduler


testcases = [
    Testcase('basic', nodes=2, ppn=2, data='./testcase/testcase1'),
    Testcase('basic', nodes=3, ppn=5, data='./testcase/testcase2'),
    Testcase('basic', nodes=1, ppn=7, data='./testcase/testcase3'),
    Testcase('basic', nodes=1, ppn=1, data='./testcase/testcase4'),
    Testcase('basic', nodes=2, ppn=6, data='./testcase/testcase5'),
    Testcase('basic', nodes=2, ppn=5, data='./testcase/testcase6'),
    Testcase('basic', nodes=3, ppn=1, data='./testcase/testcase7'),
    Testcase('basic', nodes=3, ppn=6, data='./testcase/testcase8'),
    Testcase('basic', nodes=2, ppn=12, data='./testcase/testcase9'),
    Testcase('basic', nodes=3, ppn=12, data='./testcase/testcase10'),
]


if __name__ == '__main__':
    sched = Scheduler(testcases)
    sched.start()
