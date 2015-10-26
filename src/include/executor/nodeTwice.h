#ifndef NODETWICE_H_
#define NODETWICE_H_

#include "nodes/execnodes.h"

extern TwiceState *ExecInitTwice(Twice *node, EState *estate, int eflags);
extern TupleTableSlot *ExecTwice(TwiceState *node);
extern void ExecEndTwice(TwiceState *node);

#endif /* NODETWICE_H_ */
