#include "postgres.h"
#include "executor/executor.h"
#include "executor/nodeTwice.h"
#include "executor/nodeUnique.h"
#include "utils/memutils.h"

TwiceState *
ExecInitTwice(Twice *node, EState *estate, int eflags)
{
	TwiceState *twicestate;

	/*
	 * create state structure
	 */
	twicestate = makeNode(TwiceState);
	twicestate->ps.plan = (Plan *) node;
	twicestate->ps.state = estate;

	/*
	 * Tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &twicestate->ps);

	/*
	 * then initialize outer plan
	 */
	outerPlanState(twicestate) = ExecInitNode(outerPlan(node),
											  estate,
											  eflags);

	/*
	 * twice nodes do no projections, so initialize projection info for this
	 * node appropriately, i.e. keep all attributes as they are.
	 */
	ExecAssignResultTypeFromTL(&twicestate->ps);
	twicestate->ps.ps_ProjInfo = NULL;

	/*
	 * Set output counter for each tuple to zero, s.t. we know that it is the
	 * first output overall.
	 */
	twicestate->isFirst = true;

	return twicestate;
}

TupleTableSlot *
ExecTwice(TwiceState *node) {

	TupleTableSlot *resultTupleSlot;
	TupleTableSlot *slot;
	PlanState *outerPlan;

	/*
	 * get information from the node
	 */
	outerPlan = outerPlanState(node);
	resultTupleSlot = node->ps.ps_ResultTupleSlot;

	/*
	 * Fetch a tuple from outer plan, and make it a result tuple.
	 */
	if(node->isFirst)
	{
		/*
		 * fetch a tuple from the outer subplan
		 */
		slot = ExecProcNode(outerPlan);
		if (TupIsNull(slot))
		{
			/* end of subplan, so we're done */
			ExecClearTuple(resultTupleSlot);
			return NULL;
		}
		node->isFirst = false;
		return ExecCopySlot(resultTupleSlot, slot);
	}

	/*
	 * If we used the current tuple already, copy it a second time. Do not
	 * proceed to the next tuple.
	 */
	node->isFirst = true;
	return ExecCopySlot(resultTupleSlot, resultTupleSlot);
}


void
ExecEndTwice(TwiceState *node)
{
	/* clean up tuple table */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	/* recursively clean up nodes in the plan rooted in this node */
	ExecEndNode(outerPlanState(node));
}
