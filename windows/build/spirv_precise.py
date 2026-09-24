#!/usr/bin/env python3
"""Decorate every floating-point arithmetic result of a SPIR-V module NoContraction.

    spirv_precise.py IN.spv OUT.spv

NoContraction is what GLSL's `precise` produces. LLPC (AMD's Windows driver) attaches
reassoc/contract/afn/arcp/nnan/nsz to every float operation that does not carry it, so without
it the driver may reorder sums, fuse multiply-adds and use approximate functions where the
shader source spells out an exact sequence. Same module otherwise; no new ids.
"""
import struct
import sys

FLOAT_OPS = {
    127,  # OpFNegate
    129,  # OpFAdd
    131,  # OpFSub
    133,  # OpFMul
    136,  # OpFDiv
    140,  # OpFRem
    141,  # OpFMod
    142,  # OpVectorTimesScalar
    143,  # OpMatrixTimesScalar
    144,  # OpVectorTimesMatrix
    145,  # OpMatrixTimesVector
    146,  # OpMatrixTimesMatrix
    148,  # OpDot
    12,   # OpExtInst (GLSL.std.450 Fma, Exp2, InverseSqrt, ...)
}
OP_DECORATE, NO_CONTRACTION = 71, 42
ANNOTATIONS = {71, 72, 74, 75, 332, 5632, 5633}   # OpDecorate, MemberDecorate, groups, ...Id, ...String


def main():
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, 'rb').read()
    w = list(struct.unpack('<%dI' % (len(data) // 4), data))
    header, i = w[:5], 5
    insts, results, already = [], [], set()
    while i < len(w):
        n, op = w[i] >> 16, w[i] & 0xffff
        inst = w[i:i + n]
        insts.append((op, inst))
        if op == OP_DECORATE and inst[2] == NO_CONTRACTION:
            already.add(inst[1])
        if op in FLOAT_OPS and n >= 3:
            results.append(inst[2])          # result id (inst[1] is the result type)
        i += n
    # after the last annotation, before the first type/constant declaration
    last_ann = max(k for k, (op, _) in enumerate(insts) if op in ANNOTATIONS)
    new = [[(3 << 16) | OP_DECORATE, r, NO_CONTRACTION] for r in results if r not in already]
    out = header[:]
    for k, (op, inst) in enumerate(insts):
        out += inst
        if k == last_ann:
            for d in new:
                out += d
    open(dst, 'wb').write(struct.pack('<%dI' % len(out), *out))
    print(f'{src}: {len(new)} results decorated NoContraction')


if __name__ == '__main__':
    main()
