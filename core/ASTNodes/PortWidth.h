#pragma once

// Represents a bit-width range [msb:lsb].
// Scalar signals have msb = lsb = 0 and scalar = true.
struct PortWidth {
    int  msb;
    int  lsb;
    bool scalar;  // true if no [x:y] was written (e.g. plain `input clk`)

    PortWidth() : msb(0), lsb(0), scalar(true) {}
    PortWidth(int msb, int lsb) : msb(msb), lsb(lsb), scalar(false) {}

    int width() const { return scalar ? 1 : (msb - lsb + 1); }
};
