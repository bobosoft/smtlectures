/*********************************************************************
Author: Roberto Bruttomesso <roberto.bruttomesso@gmail.com>

OpenSMT -- Copyright (C) 2010, Roberto Bruttomesso

OpenSMT is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenSMT is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenSMT. If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "Egraph.h"
#include "LA.h"

//
// DTC related routines
//
int Egraph::getInterfaceTermsNumber( )
{
  return interface_terms.size( );
}

Enode * Egraph::getInterfaceTerm( const int n )
{
  assert( n < static_cast< int >( interface_terms.size( ) ) );
  return interface_terms[ n ];
}

void Egraph::gatherInterfaceTerms( Enode * e )
{
  assert( config.sat_lazy_dtc != 0 );
  assert( config.logic == QF_UFIDL
       || config.logic == QF_UFLRA );

  assert( e );

  if ( config.verbosity > 2 )
    cerr << "# Egraph::Gathering interface terms" << endl;

  vector< Enode * > unprocessed_enodes;
  initDup1( );

  unprocessed_enodes.push_back( e );
  //
  // Visit the DAG of the term from the leaves to the root
  //
  while( !unprocessed_enodes.empty( ) )
  {
    Enode * enode = unprocessed_enodes.back( );
    // 
    // Skip if the node has already been processed before
    //
    if ( isDup1( enode ) )
    {
      unprocessed_enodes.pop_back( );
      continue;
    }

    bool unprocessed_children = false;
    Enode * arg_list;
    for ( arg_list = enode->getCdr( ) ; 
	  arg_list != enil ; 
	  arg_list = arg_list->getCdr( ) )
    {
      Enode * arg = arg_list->getCar( );
      assert( arg->isTerm( ) );
      //
      // Push only if it is unprocessed
      //
      if ( !isDup1( arg ) )
      {
	unprocessed_enodes.push_back( arg );
	unprocessed_children = true;
      }
    }
    //
    // SKip if unprocessed_children
    //
    if ( unprocessed_children )
      continue;

    unprocessed_enodes.pop_back( );                      
    //
    // At this point, every child has been processed
    //
    if ( enode->isUFOp( ) )
    {
      // Retrieve arguments
      for ( Enode * arg_list = enode->getCdr( ) 
	  ; !arg_list->isEnil( ) 
	  ; arg_list = arg_list->getCdr( ) )
      {
	Enode * arg = arg_list->getCar( );
	// This is for sure an interface term
	if ( ( arg->isArithmeticOp( ) 
	    || arg->isConstant( ) )
	  && interface_terms_cache.insert( arg ).second )
	{
	  if ( config.verbosity > 2 )
	    cerr << "# Egraph::Added interface term: " << arg << endl;
	  // Save info for backtracking
	  undo_stack_oper.push_back( INTERFACE_TERM );
	  undo_stack_term.push_back( arg );
	}
	// We add this variable to the potential
	// interface terms or to interface terms if
	// already seen in LA
	else if ( arg->isVar( ) || arg->isConstant( ) )
	{
	  if ( it_la.find( arg ) == it_la.end( ) )
	  {
	    if ( it_uf.insert( arg ).second )
	    {
	      // Insertion took place, save undo info
	      undo_stack_oper.push_back( INTERFACE_UF );
	      undo_stack_term.push_back( arg );
	    }
	  }
	  else if ( interface_terms_cache.insert( arg ).second )
	  {
	    interface_terms.push_back( arg );
	    if ( config.verbosity > 2 )
	      cerr << "# Egraph::Added interface term: " << arg << endl;
	    // Save info for backtracking
	    undo_stack_oper.push_back( INTERFACE_TERM );
	    undo_stack_term.push_back( arg );
	  }
	}
      }
    }

    if ( enode->isArithmeticOp( ) 
      && !isRootUF( enode ) )
    {
      // Retrieve arguments
      for ( Enode * arg_list = enode->getCdr( ) 
	  ; !arg_list->isEnil( ) 
	  ; arg_list = arg_list->getCdr( ) )
      {
	Enode * arg = arg_list->getCar( );
	// This is for sure an interface term
	if ( arg->isUFOp( ) 
	  && !arg->isUp( )
	  && interface_terms_cache.insert( arg ).second )
	{
	  interface_terms.push_back( arg );
	  if ( config.verbosity > 2 )
	    cerr << "# Egraph::Added interface term: " << arg << endl;
	  // Save info for backtracking
	  undo_stack_oper.push_back( INTERFACE_TERM );
	  undo_stack_term.push_back( arg );
	}
	// We add this variable to the potential
	// interface terms or to interface terms if
	// already seen in UF
	else if ( arg->isVar( ) || arg->isConstant( ) )
	{
	  if ( it_uf.find( arg ) == it_uf.end( ) )
	  {
	    if ( it_la.insert( arg ).second )
	    {
	      // Insertion took place, save undo info
	      undo_stack_oper.push_back( INTERFACE_LA );
	      undo_stack_term.push_back( arg );
	    }
	  }
	  else if ( interface_terms_cache.insert( arg ).second )
	  {
	    interface_terms.push_back( arg );
	    if ( config.verbosity > 2 )
	      cerr << "# Egraph::Added interface term: " << arg << endl;
	    // Save info for backtracking
	    undo_stack_oper.push_back( INTERFACE_TERM );
	    undo_stack_term.push_back( arg );
	  }
	}
      }
    }

    assert( !isDup1( enode ) );
    storeDup1( enode );
  }

  doneDup1( );
}

#ifdef PRODUCE_PROOF
void Egraph::getInterfaceVars( Enode * e, set< Enode * > & iv )
{
  assert( config.produce_inter != 0 );
  assert( config.sat_lazy_dtc != 0 );
  assert( config.logic == QF_UFIDL
       || config.logic == QF_UFLRA );

  assert( e );

  vector< Enode * > unprocessed_enodes;
  initDup1( );

  unprocessed_enodes.push_back( e );
  //
  // Visit the DAG of the term from the leaves to the root
  //
  while( !unprocessed_enodes.empty( ) )
  {
    Enode * enode = unprocessed_enodes.back( );
    // 
    // Skip if the node has already been processed before
    //
    if ( isDup1( enode ) )
    {
      unprocessed_enodes.pop_back( );
      continue;
    }

    bool unprocessed_children = false;
    Enode * arg_list;
    for ( arg_list = enode->getCdr( ) ; 
	  arg_list != enil ; 
	  arg_list = arg_list->getCdr( ) )
    {
      Enode * arg = arg_list->getCar( );
      assert( arg->isTerm( ) );
      //
      // Push only if it is unprocessed
      //
      if ( !isDup1( arg ) )
      {
	unprocessed_enodes.push_back( arg );
	unprocessed_children = true;
      }
    }
    //
    // SKip if unprocessed_children
    //
    if ( unprocessed_children )
      continue;

    unprocessed_enodes.pop_back( );                      

    if ( enode->isVar( )
      && interface_terms_cache.find( enode ) != interface_terms_cache.end( ) )
      iv.insert( enode );

    assert( !isDup1( enode ) );
    storeDup1( enode );
  }

  doneDup1( );
}
#endif

//
// Check if the subformula is purely
//
bool Egraph::isPureLA( Enode * e )
{
  assert( config.sat_lazy_dtc != 0 );
  assert( config.logic == QF_UFIDL
       || config.logic == QF_UFLRA );

  assert( e );
  vector< Enode * > unprocessed_enodes;
  initDup1( );

  unprocessed_enodes.push_back( e );
  //
  // Visit the DAG of the term from the leaves to the root
  //
  while( !unprocessed_enodes.empty( ) )
  {
    Enode * enode = unprocessed_enodes.back( );
    // 
    // Skip if the node has already been processed before
    //
    if ( isDup1( enode ) )
    {
      unprocessed_enodes.pop_back( );
      continue;
    }

    bool unprocessed_children = false;
    Enode * arg_list;
    for ( arg_list = enode->getCdr( ) ; 
	  arg_list != enil ; 
	  arg_list = arg_list->getCdr( ) )
    {
      Enode * arg = arg_list->getCar( );
      assert( arg->isTerm( ) );
      //
      // Push only if it is unprocessed
      //
      if ( !isDup1( arg ) )
      {
	unprocessed_enodes.push_back( arg );
	unprocessed_children = true;
      }
    }
    //
    // SKip if unprocessed_children
    //
    if ( unprocessed_children )
      continue;

    unprocessed_enodes.pop_back( );                      

    //
    // At this point, every child has been processed
    //
    if ( enode->isUFOp( ) )
    {
      doneDup1( );
      return false;
    }

    assert( !isDup1( enode ) );
    storeDup1( enode );
  }

  doneDup1( );
  return true;
}

bool Egraph::isPureUF( Enode * e )
{
  assert( config.sat_lazy_dtc != 0 );
  assert( config.logic == QF_UFIDL
       || config.logic == QF_UFLRA );

  assert( e );
  vector< Enode * > unprocessed_enodes;
  initDup1( );

  unprocessed_enodes.push_back( e );
  //
  // Visit the DAG of the term from the leaves to the root
  //
  while( !unprocessed_enodes.empty( ) )
  {
    Enode * enode = unprocessed_enodes.back( );
    // 
    // Skip if the node has already been processed before
    //
    if ( isDup1( enode ) )
    {
      unprocessed_enodes.pop_back( );
      continue;
    }

    bool unprocessed_children = false;
    Enode * arg_list;
    for ( arg_list = enode->getCdr( ) ; 
	  arg_list != enil ; 
	  arg_list = arg_list->getCdr( ) )
    {
      Enode * arg = arg_list->getCar( );
      assert( arg->isTerm( ) );
      //
      // Push only if it is unprocessed
      //
      if ( !isDup1( arg ) )
      {
	unprocessed_enodes.push_back( arg );
	unprocessed_children = true;
      }
    }
    //
    // SKip if unprocessed_children
    //
    if ( unprocessed_children )
      continue;

    unprocessed_enodes.pop_back( );                      

    //
    // At this point, every child has been processed
    //
    if ( enode->isArithmeticOp( ) )
    {
      doneDup1( );
      return false;
    }

    assert( !isDup1( enode ) );
    storeDup1( enode );
  }

  doneDup1( );
  return true;
}

bool Egraph::isRootUF( Enode * e )
{
  assert( e );
  assert( config.sat_lazy_dtc != 0 );
  assert( config.logic == QF_UFIDL
       || config.logic == QF_UFLRA );
  //
  // Makes sense only for TAtoms
  //
  if ( !e->isTAtom( ) )
    return false;
  //
  // There is an arithmetic operator
  //
  if ( e->isArithmeticOp( ) && !e->isEq( ) )
    return false;
  //
  // Uninterpreted predicates are OK
  //
  if ( e->isUp( ) )
    return true;
  
  assert( e->isEq( ) );

  Enode * x = e->get1st( );
  Enode * y = e->get2nd( );
  //
  // We want equalities of the form x=y
  //
  if ( x->isArithmeticOp( ) 
    || y->isArithmeticOp( ) )
    return false;
  //
  // And they also have to have at least a member 
  // that is not interface  
  //
  if ( interface_terms_cache.find( x ) == interface_terms_cache.end( ) 
    || interface_terms_cache.find( y ) == interface_terms_cache.end( ) )
    return true;

  return false;
}

Enode * Egraph::canonizeDTC( Enode * formula
                           , bool split_eqs )
{
  assert( config.sat_lazy_dtc != 0 );
  assert( config.logic == QF_UFLRA
       || config.logic == QF_UFIDL );

  list< Enode * > dtc_axioms;
  vector< Enode * > unprocessed_enodes;
  initDupMap1( );

  unprocessed_enodes.push_back( formula );
  //
  // Visit the DAG of the formula from the leaves to the root
  //
  while( !unprocessed_enodes.empty( ) )
  {
    Enode * enode = unprocessed_enodes.back( );
    //
    // Skip if the node has already been processed before
    //
    if ( valDupMap1( enode ) != NULL )
    {
      unprocessed_enodes.pop_back( );
      continue;
    }

    bool unprocessed_children = false;
    Enode * arg_list;
    for ( arg_list = enode->getCdr( )
	; arg_list != enil
	; arg_list = arg_list->getCdr( ) )
    {
      Enode * arg = arg_list->getCar( );
      assert( arg->isTerm( ) );
      //
      // Push only if it is unprocessed
      //
      if ( valDupMap1( arg ) == NULL )
      {
	unprocessed_enodes.push_back( arg );
	unprocessed_children = true;
      }
    }
    //
    // SKip if unprocessed_children
    //
    if ( unprocessed_children )
      continue;

    unprocessed_enodes.pop_back( );
    Enode * result = NULL;
    //
    // Replace arithmetic atoms with canonized version
    //
    if (  enode->isTAtom( ) 
      && !enode->isIff( )
      && !enode->isUp( ) )
    {
      // No need to do anything if node is purely UF
      if ( isRootUF( enode ) )
      {
	if ( config.verbosity > 2 )
	  cerr << "# Egraph::Skipping canonization of " << enode << " as it's root is purely UF" << endl;
	result = enode;
      }
      else
      {
	LAExpression a( enode );
	result = a.toEnode( *this );
      
	if ( split_eqs && result->isEq( ) )
	{
#ifdef PRODUCE_PROOF
	  if ( config.produce_inter != 0 )
	    opensmt_error2( "can't compute interpolant for equalities at the moment ", enode );
#endif
	  LAExpression aa( enode );
	  Enode * e = aa.toEnode( *this );
	  Enode * lhs = e->get1st( );
	  Enode * rhs = e->get2nd( );
	  Enode * leq = mkLeq( cons( lhs, cons( rhs ) ) );
	  LAExpression b( leq );
	  leq = b.toEnode( *this );
	  Enode * geq = mkGeq( cons( lhs, cons( rhs ) ) );
	  LAExpression c( geq );
	  geq = c.toEnode( *this );
	  Enode * not_e = mkNot( cons( enode ) );
	  Enode * not_l = mkNot( cons( leq ) );
	  Enode * not_g = mkNot( cons( geq ) );
	  // Add clause ( !x=y v x<=y )
	  Enode * c1 = mkOr( cons( not_e
		           , cons( leq ) ) );
	  // Add clause ( !x=y v x>=y )
	  Enode * c2 = mkOr( cons( not_e
		           , cons( geq ) ) );
	  // Add clause ( x=y v !x>=y v !x<=y )
	  Enode * c3 = mkOr( cons( enode
		           , cons( not_l
		           , cons( not_g ) ) ) );
	  // Add conjunction of clauses
	  Enode * ax = mkAnd( cons( c1
		            , cons( c2
		            , cons( c3 ) ) ) );

	  dtc_axioms.push_back( ax );
	  result = enode;
	}
      }
    }
    //
    // If nothing have been done copy and simplify
    //
    if ( result == NULL )
      result = copyEnodeEtypeTermWithCache( enode );

    assert( valDupMap1( enode ) == NULL );
    storeDupMap1( enode, result );
  }

  Enode * new_formula = valDupMap1( formula );
  assert( new_formula );
  doneDupMap1( );

  if ( !dtc_axioms.empty( ) )
  {
    dtc_axioms.push_back( new_formula );
    new_formula = mkAnd( cons( dtc_axioms ) );
  }

  return new_formula;
}

//
// Ackermann related routines
//
void
Egraph::retrieveFunctionApplications( Enode * formula )
{
  assert( formula );
  vector< Enode * > unprocessed_enodes;
  initDup1( );

  unprocessed_enodes.push_back( formula );

  //
  // Visit the DAG of the formula from the leaves to the root
  //
  while( !unprocessed_enodes.empty( ) )
  {
    Enode * enode = unprocessed_enodes.back( );
    // 
    // Skip if the node has already been processed before
    //
    if ( isDup1( enode ) )
    {
      unprocessed_enodes.pop_back( );
      continue;
    }

    bool unprocessed_children = false;
    Enode * arg_list;
    for ( arg_list = enode->getCdr( ) ; 
	  arg_list != enil ; 
	  arg_list = arg_list->getCdr( ) )
    {
      Enode * arg = arg_list->getCar( );

      assert( arg->isTerm( ) );
      //
      // Push only if it is unprocessed
      //
      if ( !isDup1( arg ) )
      {
	unprocessed_enodes.push_back( arg );
	unprocessed_children = true;
      }
    }
    //
    // SKip if unprocessed_children
    //
    if ( unprocessed_children )
      continue;

    unprocessed_enodes.pop_back( );                      
    //
    // At this point, every child has been processed
    //
    if ( enode->isUf( ) || enode->isUp( ) )
    {
      if ( uf_to_appl_cache[ enode->getCar( ) ].insert( enode ).second )
      {
	uf_to_appl[ enode->getCar( ) ].push_back( enode );
	undo_stack_oper.push_back( ACK_APPL );
	undo_stack_term.push_back( enode );
      }
    }

    assert( !isDup1( enode ) );
    storeDup1( enode );
  }

  doneDup1( );
}

void
Egraph::initializeAck( )
{
  for ( map< Enode *, vector< Enode * > >::iterator it = uf_to_appl.begin( )
      ; it != uf_to_appl.end( )
      ; it ++ )
  {
    uf_to_appl_count[ it->first ].first = 0;
    uf_to_appl_count[ it->first ].second = 1;
  }
}

void
Egraph::getNextAckAxioms( vector< Enode * > & axioms )
{
  //
  // Outer loop, for each function symbol
  //
  for ( map< Enode *, vector< Enode * > >::iterator it = uf_to_appl.begin( )
      ; it != uf_to_appl.end( )
      ; it ++ )
  {
    Enode * f_symb = it->first;
    vector< Enode * > & appl = it->second;
    const size_t max_index = appl.size( );
    // Retrieve current counters
    size_t i = uf_to_appl_count[ f_symb ].first;
    size_t j = uf_to_appl_count[ f_symb ].second;
    assert( i < j );
    assert( i <  max_index );
    assert( j <= max_index );
    // All axioms already returned for this symbol
    if ( i + 1 == max_index )
    {
      assert( j == max_index );
      continue;
    }
    assert( i + 1 < max_index );
    assert( j + 1 <= max_index );
    // Generate axiom
    Enode * appl_i = appl[ i ];
    Enode * appl_j = appl[ j ];
    Enode * appl_i_list = appl_i->getCdr( );
    Enode * appl_j_list = appl_j->getCdr( );

    //
    // Construct (ai_1 = aj_1 /\ ai_2 = aj_2 /\ ... /\ ai_n = aj_n) --> f( ... ) = f( ... )
    // equiv to  (ai_1 != aj_1 \/ ai_2 != aj_2 \/ ... \/ ai_n != aj_n \/ f( ... ) = f( ... ))
    //
    // equiv to  (ai_1 < aj_1 \/ ai_1 > aj_1 \/ ... \/ f( ... ) <= f( ... ) )
    // equiv to  (ai_1 < aj_1 \/ ai_1 > aj_1 \/ ... \/ f( ... ) => f( ... ) )
    //
    list< Enode * > clause;
    while ( !appl_i_list->isEnil( ) )
    {
      appl_i_list = appl_i_list->getCdr( );
      appl_j_list = appl_j_list->getCdr( );
      Enode * arg_i = appl_i_list->getCar( );
      Enode * arg_j = appl_j_list->getCar( );

      Enode * lt = mkLt( arg_i, arg_j );
      LAExpression lalt( lt );
      clause.push_back( lalt.toEnode( *this ) );

      Enode * gt = mkGt( arg_i, arg_j );
      LAExpression lagt( gt );
      clause.push_back( lagt.toEnode( *this ) );
    }
    clause.push_back( mkLeq( appl_i, appl_j ) );
    // Push first axiom
    axioms.push_back( mkOr( cons( clause ) ) );
    clause.pop_back( );
    clause.push_back( mkGeq( appl_i, appl_j ) );
    // Push second axiom
    axioms.push_back( mkOr( cons( clause ) ) );

    // Update counters
    j ++;
    if ( j == max_index ) 
    {
      i ++;
      j = i + 1;
    }
    // Store for next call
    uf_to_appl_count[ f_symb ].first = i;
    uf_to_appl_count[ f_symb ].second = j;
  }
}
