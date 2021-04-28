
SFSCON: POSTGRESQL DEVELOPMENT TALK
===================================

* Author: *Peter Moser*
* Contact: [pitiz29a@gmail.com](mailto:pitiz29a@gmail.com)
* Last modified: 2015-10-26
* License: [CC-BY-SA](http://creativecommons.org/licenses/by-sa/4.0/)


## Prerequsites to understand this tutorial

* Basic knowledge of the programming language **C**
* Some understanding of **git** (or other version control systems) 
* **GNU Autoconf** tools and **C compiler** handling (i.e., gcc, configure, ...)
* **PostgreSQL** usage (ex., querying, DDL, ...)
* Useful source for developers (not necessary for this tutorial): [PG Developer FAQ](https://wiki.postgresql.org/wiki/Developer_FAQ).

## Getting the source

I explain it with GIT, but it should be similar with every other version control
system. The current developer version is **9.6devel**, as you can see at each
`psql` output (first line).

* Fork the GIT repository (mirror) of PostgreSQL, go to
  [PostgreSQL repository](https://github.com/postgres/postgres) and fork the
  repository (you need an account for that)
* Clone it (i.e., copy it to your machine) with
  `git clone git@github.com:Piiit/postgres.git postgresql-sfscon`
  where `postgres-sfscon` is the target directory

 
  
## Configure your development environment

We will use `git`, `configure`, `gcc`, `GNU make`, `autoconf`, `gdb`, `bison`,
`flex`, and `Eclipse Kepler with CDT` on Linux.

<!-- TODO Does this really work? 
Set the environmental variable: `CFLAGS=-O0 -g` to tell the compiler that we do
not want any optimization. This makes the debugging easier later on, because the 
compiler does not change the execution of our code. We want to step through it
as we have programmed it. -->

NB: I use my own home folder throughout this tutorial, since it has been started as 
my personal notes for the workshop, not ment to be shared. So, please replace 
`/home/pemoser` with your folder (some commands need absolute paths, so no `~` 
allowed there)

We configure our system with:
    
		./configure --prefix=/home/pemoser/projects/postgresql-sfscon/server \
		--enable-debug --enable-depend --enable-cassert

The server binaries will be installed to 

		/home/pemoser/projects/postgresql-sfscon/server

If we cannot run this command, we must check if all needed packages have been 
installed. On Linux these are mostly the tools that are listed above, and some 
libraries (i.e., `libreadline-dev` and `zlib1g-dev`). 



## Import the project into Eclipse

* File > Import > Existing Code as Makefile Project ...
* Configure the properties of the project: right-click > properties...
* Add the include folder in src/include: Path & Symbols > Includes...
* Define the source folder location: Path & Symbols > Source location...



## Compile the code

Run `make` and then `make install`.



## Start the server & client to check if it runs

The current directory is: `/home/pemoser/projects/postgresql-sfscon/`.

First of all, we create a database cluster with the following command:

		./server/bin/initdb -D /home/pemoser/projects/postgresql-sfscon/data

Second, we must start the server:

		./server/bin/pg_ctl -D /home/pemoser/projects/postgresql-sfscon/data \
		-l logfile start -o "-p 5555"
    
This command starts the server with the cluster stored in `/home/pemoser/projects/postgresql-sfscon/data`, a logfile
stored in the current directory, and with a server port set to `5555` (since we 
do not want to interfer with already running instances of PostgreSQL).

Third, we create a new database named `A`: 

		./server/bin/createdb -p 5555 -h localhost A
    
We can access the database now with

		./server/bin/psql -p 5555 -h localhost -d A
    
    

## Development of a Patch

We create a simple SQL language extension, that we call `TWICE`. It duplicates
all rows, and outputs them as result. For instance, assume a database (called 
`A`), that contains a single table, named `T`. We create `T` as follows:

		CREATE TABLE T (a INT);
    
And populate it with 3 values:

		COPY T FROM STDIN;
		1
		2
		3
		\.

When an user likes to have a duplicated output, he can simply run:

		SELECT TWICE * FROM T;
		 a 
		---
		 1
		 1
		 2
		 2
		 3
		 3
		(6 rows)
	
Which is semantically similar (i.e., ordering is not preserved) to:
		TABLE T UNION ALL TABLE T;
    
This said, we can think of two solutions to create the `TWICE` extension:
* Rewrite the query above to the `union all` query
* Add new methods to the PostgreSQL kernel, that can handle the `TWICE` keyword
  correctly by itself (we will use this, because the first one touches only a 
  few phases of the query path, where this one needs a deeper insight into the 
  internals of the DBMS)
  
NB: I use `psql` as default client here. If you want to use pgAdmin instead, you 
must populate your database differently, since copy from stdin is not supported.
  
  
  
### First Phase: The parser and analyzer

We must introduce a new keyword to the PostgreSQL parser, namely `TWICE`.
Go to the parser source directory: `src/parser/gram.y`. This file contains the 
grammar used by bison to generate the parser. The same folder also has a 
`scan.l` file for lexical analysis, but we do not touch this file here.


#### Defining new grammar rules in gram.y and kwlist.h

`TWICE` will become a part of a select statement, hence we must search for that.
Our search stops at:

		simple_select:
			SELECT opt_all_clause opt_target_list ...
			
This is the defintion of the non-terminal symbol `simple_select`, which 
describes a select-statement. `opt_distinct` for ex. means that there must be a 
production rule to create an optional distinct clause. We just add an optional
twice clause afterwards: 

		SELECT opt_all_clause opt_twice_clause opt_target_list ...
    
*Please note: The following line was missing during the workshop at SFSCON15. I skipped it accidentally.*

Additional we must change the production rule to handle twice clauses:

		n->twice = $3;

and adapt all other `$n` statements to match the corresponding positions above,
for example, `n->targetList = $3;` becomes `n->targetList = $4;`.

`opt_twice_clause` is then defined as follows:

		opt_twice_clause:
			TWICE									{ $$ = TRUE; }
			| /*EMPTY*/								{ $$ = FALSE; }
		;

The `TWICE` keyword does not need detailed information for its execution, just
a simple boolean value to store if it is activated or not. Hence, we choose 
`boolean` as token type. We define this like:

		%type <boolean> opt_twice_clause

Then we must add the new keyword `TWICE` to the list of all keywords. We search
for the token `keyword`.

		%token <keyword> ABORT_P ABSOLUTE_P ACCESS ACTION ...
    
This is the list of all key words in alphabetical order. We add `TWICE` here.

		...
		TRUNCATE TRUSTED TWICE TYPE_P TYPES_P 
		...

...and to the key word list file, namely `kwlist.h`:

		PG_KEYWORD("twice", TWICE, RESERVED_KEYWORD)
	
...and the `reserved_keyword` list inside our context free grammar in `gram.y`:

		reserved_keyword:
			...
			| TRUE_P
			| TWICE
			...
		
NB: A good german book if you wanna dive deeper into parser and lexer topics is 
*Lex & yacc die Profitools zur lexikalischen und syntaktischen Textanalyse*, or
an english alternative could be *flex & bison* by John R. Levine (ISBN 
0-596-80541-1)
		
		
#### Adding functionality to a select statement

`analyze.c` transforms the query string into an internal tree representation,
that will be used through the whole parsing process. It is called parsetree,
whereas the result of this is called querytree. The parse analysis can optimize
queries, and remove or alter statements as appropriate.

Search for `transformSelectStmt`:

		static Query *
		transformSelectStmt(ParseState *pstate, SelectStmt *stmt)

`SelectStmt` is a struct, defined in `parsenodes.h`. We add a field for `TWICE`,
for instance 

		bool		twice;			/* Do you want every result tuple twice? */
	
This value is already set, because we defined it inside the production rule of
the select statement. During parse analysis we do not perform any kind of 
optimization for the `TWICE` clause, hence we can simply set this flag inside 
the querytree as it comes from the parser:

		qry->twice = stmt->twice;
	
This field must be created inside the `Query` struct as:

		bool		twice;			/* Do you want every result tuple twice? */
	


#### Testing the first phase

Now we have added `TWICE` to the parser. It can understand the new key word, and
knows where it is allowed, and where not. In addition, the parse analysis passes
the given value to the query tree. 

So, lets run `make` and `make install` to see if everything works well.

However, when we like to test it, `psql` may give an error. This is, the
because the database cluster was created with an old configuration of the
grammar, hence we must delete and rebuild it.

		./server/bin/pg_ctl -D data/ -o "-p 5555" -l logfile stop
		rm -rf data/
		./server/bin/initdb -D data/
    
Now, we can test the new grammar:

		./server/bin/pg_ctl -D data/ -o "-p 5555" -l logfile start
		./server/bin/createdb -p 5555 A
		./server/bin/psql -p 5555 -d A
    
If you do not want to create new tables all the time, you could use a `VALUES`
clause to test the `TWICE` clause.

		SELECT TWICE * FROM (VALUES(1),(2)) T;
		column1 
		---------
				1
				2
		(2 rows)

The output above tells us, that the parser accepts the grammar, but the executor
does not duplicate the rows.

In order to see how the parsetree looks internally, we can use the function
`pprint` (i.e., pretty-print), that takes PostgreSQL nodes as input and
creates a nicely intended tree of its contents. A good place to add this
function call is the entry point of PostgreSQL query handler, namely
`src/backend/tcop/postgres.c`. Search for `pg_analyze_and_rewrite` inside
`exec_simple_query`, and add the following lines before:

		printf(">>>>>>>>>>>>>>>>>>>>>>>> PARSETREE START\n\n");
		pprint(parsetree);
		printf(">>>>>>>>>>>>>>>>>>>>>>>> PARSETREE END\n\n");

NB: `pg_analyze_and_rewrite` performs parse analysis and rule rewriting, given a 
raw parsetree (gram.y output), and optionally information about types of 
parameter symbols ($n). 

The output written to the `logfile` (if configured) for the query 

		SELECT TWICE * FROM T;

is something like this:

		>>>>>>>>>>>>>>>>>>>>>>>> PARSETREE START

		   {SELECT 
		   :distinctClause <> 
		   :intoClause <> 
		   :targetList (
			  {RESTARGET 
			  :name <> 
			  :indirection <> 
			  :val 
				 {COLUMNREF 
				 :fields (
				    {A_STAR
				    }
				 )
				 :location 13
				 }
			  :location 13
			  }
		   )
		   :fromClause (
			  {RANGEVAR 
			  :schemaname <> 
			  :relname t 
			  :inhOpt 2 
			  :relpersistence p 
			  :alias <> 
			  :location 20
			  }
		   )
		   :whereClause <> 
		   :groupClause <> 
		   :havingClause <> 
		   :windowClause <> 
		   :valuesLists <> 
		   :sortClause <> 
		   :limitOffset <> 
		   :limitCount <> 
		   :lockingClause <> 
		   :withClause <> 
		   :op 0 
		   :all false 
		   :larg <> 
		   :rarg <>
		   }

		>>>>>>>>>>>>>>>>>>>>>>>> PARSETREE END
	   
NB: You may notice that there is no `twice` field shown. This is due the fact, 
that we have not defined an output rule for it yet. Please refer to the last 
chapter to see how it is solved.


### Second phase: The Optimizer/Planner

Not much to do here for us. We simply accept the input of the given query, and 
pass it from the analyzer to the optimizer. The optimizer creates then a node
for `TWICE` and adds it to the execution plan. This is the input for the 
executor.

We create the structure for a twice-plan-node in 
`src/include/nodes/plannodes.h`:

		typedef struct Twice
		{
			Plan plan;
		} Twice;
	
All plan nodes "derive" from the Plan structure by having the `Plan` structure
as the first field.  This ensures that everything works when nodes are cast
to Plan's. A plan node has two children, called `lefttree` and `righttree`,
cost fields used to hold estimated execution time, and more.  See comments
in `plannodes.h` for further details (that is always a good way to learn,
PostgreSQL provides good documentation inside its code).


We do not need additional parameters for `Twice`, hence we have only a `plan` 
with the name `Twice`. To fill this structure, we define `make_plan` in 
`src/backend/optimizer/plan/createplan.c` like this:

		Twice *
		make_twice(Plan *lefttree)
		{
			Twice	   *node = makeNode(Twice);
			Plan	   *plan = &node->plan;

			copy_plan_costsize(plan, lefttree);

			/*
			 * Charge a cpu_tuple_cost for every tuple on input, multiplied 
			 * with 2, since we output every tuple twice.
			 */
			plan->plan_rows *= 2;
			plan->total_cost += cpu_tuple_cost * plan->plan_rows;

			plan->targetlist = lefttree->targetlist;
			plan->qual = NIL;
			plan->lefttree = lefttree;
			plan->righttree = NULL;

			return node;
		}
    
Since we want to access this function from outside, we must add it to the header
`src/include/optimizer/planmain.h` as follows:

	extern Twice *make_twice(Plan *lefttree);
	
The function `makeNode` needs additional information to understand how a `Twice`
node should be created. Therefore, we must define `T_Twice`. PostgreSQL handles
most structures as nodes. Each node is derived from `Node` and can therefore be
cast to it. The minimal definition of a node is its type, which gets defined in 
`src/include/nodes/nodes.h` with the `enum NodeTag`. We search for the section
`TAGS FOR PLAN NODES` and add `T_Twice` there:

		...
		T_Unique,
		T_Gather,
		T_Twice,
		...
	
	
The last step for the planner is to actually create the node:

		/*
		 * If there is a TWICE clause, add the necessary node. We add this after
		 * the distinct node handling.
		 */
		if (parse->twice) {
			result_plan = (Plan *) make_twice(result_plan);
		}
  
  
#### Testing the second phase

Before we continue lets see what works, and how PostgreSQL reacts upon our newly
defined `Twice` node. We start `psql` first.

		psql (9.6devel)
		Type "help" for help.

		A=# \d
				List of relations
		 Schema | Name | Type  |  Owner
		--------+------+-------+---------
		 public | t    | table | pemoser
		(1 row)

		A=# select twice * from t;
		ERROR:  unrecognized node type: 134

		A=# explain select twice * from t;
					         QUERY PLAN
		-----------------------------------------------------
		 Seq Scan on t  (cost=0.00..35.50 rows=2550 width=4)
		(1 row)

		A=# \q
	
We get an error message: `ERROR:  unrecognized node type: 134`. This is,
because at some place in the code we have no "handler" for our planner node
`Twice` yet. If you want to be sure if the node type 134 is really `T_Twice`,
you can check it in `src/include/nodes/nodes.h`, for instance, Eclipse shows
you the enumeration as a mouse-over hint. Moreover, you can change the log
levels within `psql` with `set log_error_verbosity to verbose;`, which adds
more information to an error message (see `logfile`). We will resolve this
issue at the end of our patch development. That is, we will see in detail
where this bug comes from.


We put the `pprint` statement below `pg_analyze_and_rewrite` in
`src/backend/tcop/posgres.c`:

		printf(">>>>>>>>>>>>>>>>>>>>>>>> QUERYTREE START\n\n");
		pprint(querytree_list);
		printf(">>>>>>>>>>>>>>>>>>>>>>>> QUERYTREE END\n\n");

Now, given the query 

		SELECT TWICE * FROM T;
		
`pprint` outputs the following querytree (see `logfile` if configured):

			>>>>>>>>>>>>>>>>>>>>>>>> QUERYTREE START

			(
			   {QUERY 
			   :commandType 1 
			   :querySource 0 
			   :canSetTag true 
			   :utilityStmt <> 
			   :resultRelation 0 
			   :hasAggs false 
			   :hasWindowFuncs false 
			   :hasSubLinks false 
			   :hasDistinctOn false 
			   :hasRecursive false 
			   :hasModifyingCTE false 
			   :hasForUpdate false 
			   :hasRowSecurity false 
			   :cteList <> 
			   :rtable (
				  {RTE 
				  :alias <> 
				  :eref 
					 {ALIAS 
					 :aliasname t 
					 :colnames ("a")
					 }
				  :rtekind 0 
				  :relid 16385 
				  :relkind r 
				  :tablesample <> 
				  :lateral false 
				  :inh true 
				  :inFromCl true 
				  :requiredPerms 2 
				  :checkAsUser 0 
				  :selectedCols (b 9)
				  :insertedCols (b)
				  :updatedCols (b)
				  :securityQuals <>
				  }
			   )
			   :jointree 
				  {FROMEXPR 
				  :fromlist (
					 {RANGETBLREF 
					 :rtindex 1
					 }
				  )
				  :quals <>
				  }
			   :targetList (
				  {TARGETENTRY 
				  :expr 
					 {VAR 
					 :varno 1 
					 :varattno 1 
					 :vartype 23 
					 :vartypmod -1 
					 :varcollid 0 
					 :varlevelsup 0 
					 :varnoold 1 
					 :varoattno 1 
					 :location 13
					 }
				  :resno 1 
				  :resname a 
				  :ressortgroupref 0 
				  :resorigtbl 16385 
				  :resorigcol 1 
				  :resjunk false
				  }
			   )
			   :onConflict <> 
			   :returningList <> 
			   :groupClause <> 
			   :groupingSets <> 
			   :havingQual <> 
			   :windowClause <> 
			   :distinctClause <> 
			   :sortClause <> 
			   :limitOffset <> 
			   :limitCount <> 
			   :rowMarks <> 
			   :setOperations <> 
			   :constraintDeps <>
			   }
			)

			ERROR:  unrecognized node type: 134
			STATEMENT:  select twice * from t;
			>>>>>>>>>>>>>>>>>>>>>>>> QUERYTREE END

### Third phase: The executor

This is the last phase, the executor function for our `Twice` patch
takes tuples from lower nodes, and outputs them twice. To implement this
functionality, we must write three different functions and store them in
`src/include/executor/nodeTwice.h` and `src/backend/executor/nodeTwice.c`. The 
executor does not automatically understand that we have new routines installed. 
The source file `execProcnode.c` is the main executor entry point. We will have
a look at it at the end of this chapter.

#### Implementing our algorithm

`nodeTwice.h` looks like this:

		#ifndef NODETWICE_H_
		#define NODETWICE_H_

		#include "nodes/execnodes.h"

		extern TwiceState *ExecInitTwice(Twice *node, EState *estate, int eflags);
		extern TupleTableSlot *ExecTwice(TwiceState *node);
		extern void ExecEndTwice(TwiceState *node);

		#endif /* NODETWICE_H_ */
		
Whereas, `nodeTwice.c` looks like this:

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
			 * twice nodes do no projections, so initialize projection info for 
			 * this node appropriately, i.e. keep all attributes as they are.
			 */
			ExecAssignResultTypeFromTL(&twicestate->ps);
			twicestate->ps.ps_ProjInfo = NULL;

			/*
     			 * We have not started yet, hence it is the first time we will see tuple #1.
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
			 * If we used the current tuple already, copy it a second time. Do 
			 * not proceed to the next tuple.
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
		
Additionally, we must define the `TwiceState` struct in
`src/include/nodes/execnodes.h` as follows (for instance, below `UniqueState`):

		typedef struct TwiceState
		{
			PlanState ps;
			bool isFirst;
		} TwiceState;
		
It is not obvious at first glance, that a `PlanState` is a PostgreSQL `Node`. 
However, a `PlanState` has a field called `type` from type `NodeTag`, which 
defines a node. Hence, we must also add our `TwiceState` to 
`src/include/nodes/nodes.h`. Search for the comment `TAGS FOR PLAN STATE NODES`,
and add `T_TwiceState` to the end of that list, for example as follows:

		...
		T_LockRowsState,
		T_LimitState,
		T_TwiceState,
		...
		
#### Adding our new algorithm to the compilation list
		
We have created a new file, namely `nodeTwice.c` that must be compiled, hence we
must add it to the corresponding makefile `src/backend/executor/Makefile`.

		#-------------------------------------------------------------------------
		#
		# Makefile--
		#    Makefile for executor
		#
		# IDENTIFICATION
		#    src/backend/executor/Makefile
		#
		#-------------------------------------------------------------------------

		subdir = src/backend/executor
		top_builddir = ../../..
		include $(top_builddir)/src/Makefile.global

		OBJS = execAmi.o execCurrent.o execGrouping.o execIndexing.o execJunk.o \
			   execMain.o execParallel.o execProcnode.o execQual.o \
			   execScan.o execTuples.o \
			   execUtils.o functions.o instrument.o nodeAppend.o nodeAgg.o \
			   nodeBitmapAnd.o nodeBitmapOr.o \
			   nodeBitmapHeapscan.o nodeBitmapIndexscan.o nodeCustom.o nodeGather.o \
			   nodeHash.o nodeHashjoin.o nodeIndexscan.o nodeIndexonlyscan.o \
			   nodeLimit.o nodeLockRows.o \
			   nodeMaterial.o nodeMergeAppend.o nodeMergejoin.o nodeModifyTable.o \
			   nodeNestloop.o nodeFunctionscan.o nodeRecursiveunion.o nodeResult.o \
			   nodeSamplescan.o nodeSeqscan.o nodeSetOp.o nodeSort.o nodeUnique.o \
			   nodeValuesscan.o nodeCtescan.o nodeWorktablescan.o nodeTwice.o \
			   nodeGroup.o nodeSubplan.o nodeSubqueryscan.o nodeTidscan.o \
			   nodeForeignscan.o nodeWindowAgg.o tstoreReceiver.o tqueue.o spi.o

		include $(top_srcdir)/src/backend/common.mk

Please note the object `nodeTwice.o` within the `OBJS` list.


#### Let PostgreSQL call the right dispatch functions

Lets extract some information from the comments of 
`src/backend/executor/execProcnode.c`:

		 execProcnode.c
		   contains dispatch functions which call the appropriate "initialize",
		   "get a tuple", and "cleanup" routines for the given node type.
		   If the node has children, then it will presumably call ExecInitNode,
		   ExecProcNode, or ExecEndNode on its subnodes and do the appropriate
		   processing.
		
		[...] 
		
		 INTERFACE ROUTINES
		 ExecInitNode	-		initialize a plan node and its subplans
		 ExecProcNode	-		get a tuple by executing the plan node
		 ExecEndNode	-		shut down a plan node and its subplans
		 
		[...]
		 
		 EXAMPLE
		 Suppose we want the age of the manager of the shoe department and
		 the number of employees in that department.  So we have the query:
		 
		 		select DEPT.no_emps, EMP.age
		 		from DEPT, EMP
		 		where EMP.name = DEPT.mgr and
		 			  DEPT.name = "shoe"
		 
		 Suppose the planner gives us the following plan:
		 
		 				Nest Loop (DEPT.mgr = EMP.name)
		 				/		\
		 			   /		 \
		 		   Seq Scan		Seq Scan
		 			DEPT		  EMP
		 		(name = "shoe")
		 
		 ExecutorStart() is called first. It calls InitPlan() which
		 calls ExecInitNode() on the root of the plan -- the nest
		 loop node.
		 
		   * ExecInitNode() notices that it is looking at a nest loop and
		 	 as the code below demonstrates, it calls ExecInitNestLoop().
			 Eventually this calls ExecInitNode() on the right and left subplans
			 and so forth until the entire plan is initialized.  The result
			 of ExecInitNode() is a plan state tree built with the same structure
			 as the underlying plan tree.
		 
		   * Then when ExecutorRun() is called, it calls ExecutePlan() which calls
			 ExecProcNode() repeatedly on the top node of the plan state tree.
			 Each time this happens, ExecProcNode() will end up calling
			 ExecNestLoop(), which calls ExecProcNode() on its subplans.
			 Each of these subplans is a sequential scan so ExecSeqScan() is
			 called.  The slots returned by ExecSeqScan() may contain
			 tuples which contain the attributes ExecNestLoop() uses to
			 form the tuples it returns.
		 
		   * Eventually ExecSeqScan() stops returning tuples and the nest
			 loop join ends.  Lastly, ExecutorEnd() calls ExecEndNode() which
			 calls ExecEndNestLoop() which in turn calls ExecEndNode() on
			 its subplans which result in ExecEndSeqScan().
		 
		 This should show how the executor works by having
		 ExecInitNode(), ExecProcNode() and ExecEndNode() dispatch
		 their work to the appopriate node support routines which may
		 in turn call these routines themselves on their subplans.
		 
So what we need to do now, is to add our three functions

		ExecInitTwice
		ExecTwice
		ExecEndTwice
		
to `execProcnode.c` as you see below:

We add 

		case T_TwiceState:
			result = ExecTwice((TwiceState *) node);
			break;
			
to `execProcNode`, 

		case T_Twice:
		    result = (PlanState *) ExecInitTwice((Twice *)node,
		    									 estate, eflags);
	        break;
	        
to `ExecInitNode`, and

		case T_TwiceState:
			ExecEndTwice((TwiceState *) node);
			break;

to `ExecEndNode`.

NB: Do not forget to add the `#include` for the executor node `Twice` with

		#include "executor/nodeTwice.h"
		
The plan tree, after inserting the following lines in `postgres.c` below the 
function call to `pg_plan_queries`

		printf(">>>>>>>>>>>>>>>>>>>>>>>> PLANTREE START\n\n");
		pprint(plantree_list);
		printf(">>>>>>>>>>>>>>>>>>>>>>>> PLANTREE END\n\n");

looks then like this:

		>>>>>>>>>>>>>>>>>>>>>>>> PLANTREE START

		(
		   {PLANNEDSTMT 
		   :commandType 1 
		   :queryId 0 
		   :hasReturning false 
		   :hasModifyingCTE false 
		   :canSetTag true 
		   :transientPlan false 
		   :planTree 
			  {
			  }
		   :rtable (
			  {RTE 
			  :alias <> 
			  :eref 
				 {ALIAS 
				 :aliasname t 
				 :colnames ("a")
				 }
			  :rtekind 0 
			  :relid 16385 
			  :relkind r 
			  :tablesample <> 
			  :lateral false 
			  :inh false 
			  :inFromCl true 
			  :requiredPerms 2 
			  :checkAsUser 0 
			  :selectedCols (b 9)
			  :insertedCols (b)
			  :updatedCols (b)
			  :securityQuals <>
			  }
		   )
		   :resultRelations <> 
		   :utilityStmt <> 
		   :subplans <> 
		   :rewindPlanIDs (b)
		   :rowMarks <> 
		   :relationOids (o 16385)
		   :invalItems <> 
		   :nParamExec 0 
		   :hasRowSecurity false 
		   :parallelModeNeeded false
		   }
		)

		>>>>>>>>>>>>>>>>>>>>>>>> PLANTREE END

		
#### Now lets test the patch and fix the last bugs

We run `make`, `make install`, restart the server, and start `psql` according to
our configuration.

		psql (9.6devel)
		Type "help" for help.

		A=# select twice * from t;
		ERROR:  unrecognized node type: 134
		A=# set log_error_verbosity TO verbose;
		WARNING:  could not dump unrecognized node type: 742
		SET
		A=# select twice * from t;
		ERROR:  unrecognized node type: 134
		A=# \q
		
So our patch does still not work. The problem is that at some place PostgreSQL
handels all nodes according to its type. To find this place, we issue the
command `set log_error_verbosity TO verbose;` in `psql`, s.t. we have more
information within our log file, which reveals the error *and* its position:

		ERROR:  XX000: unrecognized node type: 134
		LOCATION:  set_plan_refs, setrefs.c:901
		STATEMENT:  select twice * from t;
		
`setrefs.c` at line 901 in function `set_plan_refs` triggers this error. To
understand this, we follow the call history of `set_plan_refs` and find out
that it gets called by `set_plan_references`, located in `setrefs.c`. The
comments there teach us about the reasons of this function. Please refer
to that documentation for further details. To make our patch work, we just
add `T_Twice` below `T_Unique`, since its similar in the handling of its
targetlist. Please note, that this step makes part of the optimizer/planner 
phase not the executor phase. However, I like to show it at the end of our 
patch development in order to have a full debugging example.

		...
		case T_Hash:
		case T_Material:
		case T_Sort:
		case T_Unique:
		case T_Twice:	/* Twice node handling */
		case T_SetOp:
		case T_Gather:

			/*
			 * These plan types don't actually bother to evaluate their
			 * targetlists, because they just return their unmodified input
			 * tuples.  Even though the targetlist won't be used by the
			 * executor, we fix it up for possible use by EXPLAIN (not to
			 * mention ease of debugging --- wrong varnos are very confusing).
			 */
			set_dummy_tlist_references(plan, rtoffset);

			/*
			 * Since these plan types don't check quals either, we should not
			 * find any qual expression attached to them.
			 */
			Assert(plan->qual == NIL);
			break;
		...

After fixing the bug, we run `make` again, and see if it works.

		psql (9.6devel)
		Type "help" for help.

		A=# select twice * from t;
		 a 
		---
		(0 rows)

		A=# copy t from stdin;
		WARNING:  could not dump unrecognized node type: 715
		Enter data to be copied followed by a newline.
		End with a backslash and a period on a line by itself.
		>> 1
		>> 2
		>> 3
		>> \.
		COPY 3
		A=# select twice * from t;
		 a 
		---
		 1
		 1
		 2
		 2
		 3
		 3
		(6 rows)

		A=# \q
		
To complete the patch lets get rid of the warn message `could not dump
unrecognized node type`.  All parse tree nodes must have an output function
and a read function, defined in `src/backend/nodes/outfuncs.c` like this

		static void
		_outTwice(StringInfo str, const Twice *node)
		{
			WRITE_NODE_TYPE("TWICE");
			_outPlanInfo(str, (const Plan *) node);
		}
		
and called in `_outNode` as follows:

		case T_Twice:
			_outTwice(str, obj);
			break;
	
As you remember we have added also a `twice` field to `SelectStmt` and
`Query`. Both of them can be written to stdout. We add a output function for
`twice` accordingly. First for `SelectStmt`,

		...
		WRITE_BOOL_FIELD(all);
		WRITE_BOOL_FIELD(twice);
		WRITE_NODE_FIELD(larg);
		...
		
and secondly, for the `Query` struct:

		...
		WRITE_BOOL_FIELD(hasRowSecurity);
		WRITE_BOOL_FIELD(twice);
		WRITE_NODE_FIELD(cteList); 
		...

Additionally, we must change `src/backend/nodes/readfuncs.c` to teach
`_readQuery` how to deal with the new field `twice`.

		...
		READ_BOOL_FIELD(hasRowSecurity);
		READ_BOOL_FIELD(twice);
		READ_NODE_FIELD(cteList);
		...
		
Please refer to the comments in both files for further details. We do not add
`twiceState` handling here, because it is an executor state node. Such nodes
are never read in. There are additional node handling functions.  You should
have a look at them in `src/backend/nodes`, especially the comments at
beginning of each file contain a lot of useful information. To check this last 
changes, compile and have a look at the logfile after submitting our standard
test query:

		SELECT TWICE * FROM T;
		
You should find some boolean fields called `twice` in your query- and parse 
trees. The plantree should contain a full-fletched plan node, named `TWICE`:

		(
		   {PLANNEDSTMT 
		   :commandType 1 
		   :queryId 0 
		   :hasReturning false 
		   :hasModifyingCTE false 
		   :canSetTag true 
		   :transientPlan false 
		   :planTree 
			  {TWICE 
			  :startup_cost 0.00 
			  :total_cost 86.50 
			  :plan_rows 5100 
			  :plan_width 4 
			  :plan_node_id 0 
			  :targetlist (
				 {TARGETENTRY 
				 :expr 
				    {VAR 
				    :varno 65001 
				    :varattno 1 
				    :vartype 23 
				    :vartypmod -1 
				    :varcollid 0 
				    :varlevelsup 0 
				    :varnoold 1 
				    :varoattno 1 
				    :location -1
				    }
				 :resno 1 
				 :resname a 
				 :ressortgroupref 0 
				 :resorigtbl 16385 
				 :resorigcol 1 
				 :resjunk false
				 }
			  )
			  :qual <> 
			  :lefttree 
				 {SEQSCAN 
				 [...]
		)
		
NB: This was just a simple debugging example, for more complicated cases
it is recommended to use `gdb`, `Valgrind`, or similar debugging facilities
(see PostgreSQL wiki for developers for further information).

#### Cleanup

You should 

* remove all `pprint`s to clean up
* run a `make clean`, and `make`
* delete your data folder in order to see if `initdb` still works
* and setup a clean instance of **PostgreSQL** with **TWICE** support ;-)

NB: If you still get some `WARNING:  could not dump unrecognized node type: NNN` 
log messages, you probably forget some `pprint` somewhere. Remove them, and your 
safe! :-)

----
## We`re done!!

That's it!! We have now developed our first wonderful PostgreSQL patch. If
you want to continue by your self, just refer to the source code comments,
the online developer wiki, and manuals. I wish you fun while making your
first experiences. You can also write to the mailinglist, which is powered
by a very helpful and friendly community. :)


**Happy Hacking!**

	~piiit

PS. Feedback appreciated!! See e-mail address at the top of this article.
		


