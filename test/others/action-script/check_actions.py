#!/usr/bin/env python3

import os
import sys

EXPECTED_ACTIONS = [
    'pre-dump',
    'network-lock',
    'post-dump',
    'pre-restore',
    'setup-namespaces',
    'post-setup-namespaces',
    'post-restore',
    'network-unlock',
    'pre-resume',
    'post-resume',
]

# post-mounts fires from the root task's restore context inside the
# CR_STATE_PREPARE_NAMESPACES stage, so it races with the main process
# firing post-setup-namespaces: it is guaranteed to appear after
# setup-namespaces and before post-restore, but its position relative
# to post-setup-namespaces is timing-dependent.
RACY_ACTION = 'post-mounts'

errors = []
actions_called = []
actions_called_file = os.path.join(os.path.dirname(__file__), 'actions_called.txt')

with open(actions_called_file) as f:
    for index, line in enumerate(f):
        parts = line.strip().split()
        parts += ['EMPTY'] * (3 - len(parts))
        action_hook, image_dir, pid = parts

        if action_hook == 'EMPTY':
            raise ValueError("Error in test: bogus actions line")

        if image_dir == 'EMPTY':
            errors.append(f'Action {action_hook} misses CRTOOLS_IMAGE_DIR')

        if action_hook != 'pre-restore':
            if pid == 'EMPTY':
                errors.append(f'Action {action_hook} misses CRTOOLS_INIT_PID')
            elif not pid.isdigit() or int(pid) == 0:
                errors.append(f'Action {action_hook} PID is not a valid number ({pid})')

        actions_called.append(action_hook)

main_actions = [a for a in actions_called if a != RACY_ACTION]
if main_actions != EXPECTED_ACTIONS:
    errors.append(f'Not all actions called: {main_actions!r}')

racy_count = actions_called.count(RACY_ACTION)
if racy_count != 1:
    errors.append(f'{RACY_ACTION} called {racy_count} times, expected once')
else:
    racy_index = actions_called.index(RACY_ACTION)
    if racy_index <= actions_called.index('setup-namespaces'):
        errors.append(f'{RACY_ACTION} called before setup-namespaces')
    if racy_index >= actions_called.index('post-restore'):
        errors.append(f'{RACY_ACTION} called after post-restore')

if errors:
    print('\n'.join(errors))
    sys.exit(1)

print('Check Actions PASS')
