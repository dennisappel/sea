//
// Created by jmeintrup on 10.05.18.
//
#include "sealib/simplerankselect.h"

unsigned long Sealib::SimpleRankSelect::select(unsigned long k) const {
    if(k == 0)
        return boost::dynamic_bitset<>::npos;

    unsigned long i = bitset.find_first();
    unsigned long cnt = 1;
    if (cnt == k) return i == boost::dynamic_bitset<unsigned char>::npos ? i : i + 1;
    while ((i = bitset.find_next(i)) != boost::dynamic_bitset<unsigned char>::npos) {
        if (++cnt == k) break;
    }
    return i == boost::dynamic_bitset<unsigned char>::npos ? i : i + 1;
}

unsigned long Sealib::SimpleRankSelect::rank(unsigned long k) const {
    if(k == 0) {
        return boost::dynamic_bitset<>::npos;
    }
    if (bitset.size() < k) {
        return boost::dynamic_bitset<>::npos;
    }
    unsigned long rank = 0;
    for (unsigned long i = 0; i < k; i++) {
        if (bitset[i]) rank++;
    }
    return rank;
}

Sealib::SimpleRankSelect::SimpleRankSelect(const boost::dynamic_bitset<unsigned char> &bitset_) : bitset(bitset_) {}

Sealib::SimpleRankSelect::SimpleRankSelect(){}