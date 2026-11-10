import lfr, numpy as np
D = lfr.load_3mf()
def part(name, color=None):
    V, F = D[name]
    c = color or {"top_shell": lfr.GOLD, "base": lfr.BLACK, "clip_front": "#555a63", "clip_back": "#555a63"}[name]
    return lfr.Mesh(V, F, c, name)
def board_wf(highlight=None):
    return [lfr.bf_to_wf(m) for m in lfr.board_bf(highlight)]
