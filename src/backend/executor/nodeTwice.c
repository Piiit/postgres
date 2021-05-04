#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeTwice.h"
#include "miscadmin.h"
#include "utils/memutils.h"


static TupleTableSlot *
ExecTwice(PlanState *pstate) 
{
	TwiceState *node = castNode(TwiceState, pstate);
	TupleTableSlot *resultTupleSlot;		// The slot where we write our result into (we return it twice)
	TupleTableSlot *slot;					// The slot that we get from the outer plan node
	PlanState *outerPlan;

	CHECK_FOR_INTERRUPTS();

	/*
	 * get information from the node
	 */
	outerPlan = outerPlanState(node);
	resultTupleSlot = node->ps.ps_ResultTupleSlot;

	/*
	 * Fetch a tuple from outer plan, and make it a result tuple by copying it
	 * into the result tuple slot.
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
	 * If we used the current tuple already, just return it as-is. Do not
	 * proceed to the next tuple.
	 */
	node->isFirst = true;
	return resultTupleSlot;
}

TwiceState *
ExecInitTwice(Twice *node, EState *estate, int eflags)
{
	TwiceState *twicestate;
	Plan	   *outerPlan;

	/* An executor processes a tree of plan nodes.   Each plan nodes has its own
	 * state structure, to hold the current processing state. 
	 */
	twicestate = makeNode(TwiceState);			
	twicestate->ps.plan = (Plan *) node;
	twicestate->ps.state = estate;
	twicestate->ps.ExecProcNode = ExecTwice;

	/* Each plan node except the leaf nodes (mostly scanners) have two child
	 * nodes.   A left (or outer) plan node and a right (or inner) plan node.
	 * The Twice node has only one outer node, from which we pull tuples on
	 * demand.
	 */
	outerPlan = outerPlan(node);
	outerPlanState(twicestate) = ExecInitNode(outerPlan, estate, eflags);

	/*
	 * Initialize result slot and type.  Twice nodes do no projections, so
	 * initialize projection info for this node appropriately, i.e. keep all
	 * attributes as they are.
	 */
	ExecInitResultTupleSlotTL(&twicestate->ps, &TTSOpsMinimalTuple);
	twicestate->ps.ps_ProjInfo = NULL;

	/*
	 * Initialize our twice state internals.  That is, the first output of each
	 * tuple is true at each new tuple from the outer plan, and false on the
	 * same tuple when copied to the output tuple slot.
	 */
	twicestate->isFirst = true;

	return twicestate;
}


void
ExecEndTwice(TwiceState *node)
{
	/* clean up tuple table slot's content */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	/* recursively clean up nodes in the plan rooted in this node */
	ExecEndNode(outerPlanState(node));
}
