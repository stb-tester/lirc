#!/usr/bin/env python3

''' Simple lirc config files sanity tool. '''

import glob
import yaml

def main():
    configs = {}
    for path in glob.glob('*.conf'):
        with open(path) as f:
            cf = yaml.load(f.read())
        if cf['config']['id'] + '.conf' != path:
            print( "Id: %s, path: %s" % (cf['config']['id'], path))
        configs[cf['config']['id']] = cf['config']
    for config in configs:
        if 'supports' in config:
            if config['supports'] == 'lirccode':
                if not 'lircd_conf' in config:
                    print('bad config:' + config['id'])
    for l in configs.keys():
        if not 'menu' in configs[l]:
            print("No menu: " + l)


if __name__ == '__main__':
    main()


# vim: set expandtab ts=4 sw=4:
