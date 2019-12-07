#!/usr/bin/env python
import gerberex
from gerberex import DxfFile, GerberComposition, DrillComposition

exts = ['GTL', 'GTO', 'GTS', 'GBL', 'GBO', 'GBS']
boards=[
    ('../../mainboard/CAMOutputs/elflet.', 0, 0, 0),
]
outline = gerberex.read('../../mainboard/CAMOutputs/elflet.GML')
outputs = 'outputs/pcb'

for ext in exts:
    print('merging %s: ' % ext ,end='', flush=True)
    if ext == 'TXT':
        ctx = DrillComposition()
    else:
        ctx = GerberComposition()
    for board in boards:
        file = gerberex.read(board[0] + ext)
        file.to_metric()
        if ext == 'GTO' or ext == 'GBO':
            for adef in file.aperture_defs:
                if adef.shape == 'C' and adef.modifiers[0][0] < 0.12:
                    adef.modifiers[0] = (0.12,)
                elif adef.shape == 'R':
                    adef.modifiers[0] = (
                        adef.modifiers[0][0],
                        adef.modifiers[0][1] * 1.5,
                    )
                    if adef.modifiers[0][1] < 0.05:
                        adef.modifiers[0] = (adef.modifiers[0][0], 0.05)
        file.rotate(board[3])
        file.offset(board[1], board[2])
        ctx.merge(file)
        print('.', end='', flush=True)
    if ext == 'TXT':
        pass
    else:
        ctx.merge(outline)
    ctx.dump(outputs + '.' + ext)
    print(' end', flush=True)
