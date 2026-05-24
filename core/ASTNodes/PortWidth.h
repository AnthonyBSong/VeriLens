#pragma once
#include <string>

// Represents a bit-width range [msb:lsb].
// Scalar signals have msb = lsb = 0 and scalar = true.
// Parametric / complex widths (e.g. [N-1:0], [$clog2(N)-1:0]) keep their raw
// text in `expr` and set `unknown = true`; width() then returns 0 and callers
// (notably the validator) must skip width comparisons for them.
class PortWidth {
public:
    int         msb;
    int         lsb;
    bool        scalar;   // true if no [x:y] was written (e.g. plain `input clk`)
    bool        unknown;  // true if the range could not be reduced to integers
    std::string expr;     // raw range text when unknown, empty otherwise

    PortWidth() : msb(0), lsb(0), scalar(true), unknown(false) {}
    PortWidth(int msb, int lsb) : msb(msb), lsb(lsb), scalar(false), unknown(false) {}

    static PortWidth Unknown(const std::string& raw) {
        PortWidth w;
        w.msb = 0;
        w.lsb = 0;
        w.scalar = false;
        w.unknown = true;
        w.expr = raw;
        return w;
    }

    int width() const {
        if (unknown) return 0;
        return scalar ? 1 : (msb - lsb + 1);
    }
};
