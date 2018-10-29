#include "sealib/reversedfs.h"
#include <cmath>

using namespace Sealib;  // NOLINT

void ReverseDFS::storeTime(unsigned df, uint u) {
    if (df == 0) {
        if (d.get(u) == r + 1) {
            d.insert(u, j);
        }
    } else if (df == 1) {
        if (f.get(u) == r + 1) {
            f.insert(u, j);
        }
    }
}

void ReverseDFS::updateInterval(uint actions, bool end) {
    IntervalData &ci = i[j];
    Pair top = s.top();
    if (ci.size == 0 && ci.inUse == false) {
        ci.h1 = top;
        ci.inUse = true;
    }
    ci.size += actions;
    if (ns < ci.hdc && s.top().head() != INVALID) {
        ci.hd = top;
        ci.hdc = ns;
    }
    if (ci.size > w || end) {
        ci.h2 = top;
        ci.c1 = firstCall;
        firstCall = UserCall();
        j++;
        if (j >= r) throw std::out_of_range("too many intervals");
        updateInterval(0);
    }
}
void ReverseDFS::setCall(UserCall call) {
    if (firstCall == UserCall()) {
        firstCall = call;
    }
}

void ReverseDFS::process_recording(uint u0) {
    updateInterval(0);
    s.push(Pair(u0, 0));
    ns = 1;
    Pair x;
    while (!s.isEmpty()) {
        uint actions = 0;
        int sr = s.pop(&x);
        if (sr == DFS_DO_RESTORE) {
            restore_top(u0, g, &c, &s);
            s.pop(&x);
        } else if (sr == DFS_NO_MORE_NODES) {
            return;
        }
        uint u = x.head(), k = x.tail();
        if (k == 0 && c.get(u) == DFS_WHITE) {
            setCall({UserCall::preprocess, u});
            storeTime(0, u);
            c.insert(u, DFS_GRAY);
            actions++;
        }
        if (k < g->getNodeDegree(u)) {
            s.push(Pair(u, k + 1));
            uint v = g->head(u, k);
            setCall({UserCall::preexplore, u, k});
            if (c.get(v) == DFS_WHITE) {
                s.push(Pair(v, 0));
                actions++;
                ns++;
            }
            // (postexplore can never be a first call in an interval)
        } else {
            c.insert(u, DFS_BLACK);
            storeTime(1, u);
            setCall({UserCall::postprocess, u});
            actions++;
            ns--;
        }
        updateInterval(actions);
    }
}

void ReverseDFS::init() {
    for (uint u = 0; u < n; u++) {
        if (c.get(u) == DFS_WHITE) process_recording(u);
    }
    updateInterval(0, true);
    while (i[j].size == 0) j--;  // discard empty intervals
}

std::stack<Pair> ReverseDFS::reconstructPart(Pair from, Pair to) {
    std::stack<Pair> sj;
    std::set<uint> tmp;
    if (to.head() != INVALID) {
        if (from.head() == INVALID) {
            sj.push(to);
        } else {
            Pair a = Pair(from.head(), 0);
            tmp.insert(a.head());
            do {
                uint u = a.head();
                if (u == to.head()) {
                    sj.push(to);
                } else {
                    bool found = false;
                    for (uint k = a.tail(); k < g->getNodeDegree(u); k++) {
                        uint v = g->head(u, k);
                        if (f.get(v) == j && tmp.find(v) == tmp.end()) {
                            // using a temp. set to avoid cycles
                            a = Pair(v, 0);
                            sj.push(Pair(u, k + 1));
                            tmp.insert(v);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        throw std::logic_error(
                            "no white path found in reconstruction");
                    }
                }
            } while (sj.top() != to);
        }
    }
    return sj;
}

std::vector<UserCall> ReverseDFS::simulate(std::stack<Pair> *const sj,
                                           Pair until, UserCall first) {
    std::vector<UserCall> ret;

    if (sj->empty()) {
        if (first.type == UserCall::preprocess) {
            sj->push(Pair(first.u, 0));
        } else if (first.type == UserCall::preexplore) {
            sj->push(Pair(first.u, first.k));
        } else {
            throw std::logic_error("stack empty");
        }
    }

    while (sj->top() != until) {
        Pair x = sj->top();
        sj->pop();
        uint u = x.head(), k = x.tail();
        if (k == 0 && c.get(u) == DFS_WHITE) {
            ret.emplace_back(UserCall(UserCall::Type::preprocess, u));
            c.insert(u, DFS_GRAY);
        }
        if (k < g->getNodeDegree(u)) {
            sj->push(Pair(u, k + 1));
            uint v = g->head(u, k);
            if (c.get(v) == DFS_WHITE) {
                ret.emplace_back(UserCall(UserCall::Type::preexplore, u, k));
                sj->push(Pair(v, 0));
            } else {
                // no postexplore: will be simulated later
            }
        } else {
            c.insert(u, DFS_BLACK);
            ret.emplace_back(UserCall(UserCall::Type::postprocess, u));
            if (sj->size() > 0) {
                uint pu = sj->top().head(), pk = sj->top().tail();
                ret.emplace_back(
                    UserCall(UserCall::Type::postexplore, pu,
                             pk - 1));  // this postexplore is a major call
            }
        }
        if (sj->empty()) {
            bool a = false;
            for (uint b = 0; b < n; b++) {
                if (c.get(b) == DFS_WHITE && d.get(b) == j) {
                    sj->push(Pair(b, 0));
                    a = true;
                    break;
                } else if (c.get(b) == DFS_GRAY && f.get(b) == j) {
                    sj->push(Pair(b, g->getNodeDegree(b)));
                    a = true;
                    break;
                }
            }
            if (!a) break;
        }
    }
    return ret;
}

bool ReverseDFS::more() { return !(j == 0 && majorI == major.rend()); }

UserCall ReverseDFS::next() {
    if (majorI != major.rend()) {
        static uint mu = INVALID, mk = 0;
        static bool delayedPostexp = false;
        UserCall &cc = *majorI;
        UserCall ret;
        if (delayedPostexp) {
            delayedPostexp = false;
            ret = UserCall(UserCall::postexplore, cc.u, g->head(cc.u, mk));
        } else if (previous.u == cc.u && (cc.type == UserCall::preprocess ||
                                          cc.type == UserCall::postexplore)) {
            if (mu == INVALID) {
                mk = previous.k - 1;
            } else {
                mk--;
            }
            if (static_cast<int>(mk) >= 0) {
                uint v = g->head(cc.u, mk);
                delayedPostexp = true;
                ret = UserCall(UserCall::preexplore, cc.u, v);
            } else {
                mu = INVALID;
            }
        }
        if (mu == INVALID) {
            previous = *majorI;
            ret = *(majorI++);
        }
        return ret;
    } else {  // build new sequence
        for (uint a = 0; a < n; a++) {
            if (f.get(a) < j) {
                c.insert(a, DFS_BLACK);
            } else if (d.get(a) < j) {
                c.insert(a, DFS_GRAY);
            } else {
                c.insert(a, DFS_WHITE);
            }
        }
        std::stack<Pair> sj = reconstructPart(i[j].hd, i[j].h1);
        major = simulate(&sj, i[j].h2, i[j].c1);
        majorI = major.rbegin();
        j--;
        return next();
    }
}

ReverseDFS::ReverseDFS(Graph *graph)
    : g(graph),
      n(g->getOrder()),
      r(static_cast<uint>(ceil(8 * log2(n)))),
      w(static_cast<uint>(ceil(n / log2(n)))),
      c(n, 3),
      d(n, r + 1),
      f(n, r + 1),
      s(n, g, &c),
      i(new IntervalData[r]),
      major{0},
      majorI(major.rend()) {
    for (uint a = 0; a < n; a++) {
        c.insert(a, 0);
        d.insert(a, r + 1);
        f.insert(a, r + 1);
    }
}

ReverseDFS::~ReverseDFS() { delete[] i; }
