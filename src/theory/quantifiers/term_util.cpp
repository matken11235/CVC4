/*********************                                                        */
/*! \file term_util.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Andrew Reynolds
 ** This file is part of the CVC4 project.
 ** Copyright (c) 2009-2017 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved.  See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **
 ** \brief Implementation of term utilities class
 **/

#include "theory/quantifiers/term_util.h"

#include "expr/datatype.h"
#include "options/base_options.h"
#include "options/quantifiers_options.h"
#include "options/datatypes_options.h"
#include "options/uf_options.h"
#include "theory/theory_engine.h"
#include "theory/quantifiers_engine.h"
#include "theory/quantifiers/term_database.h"

using namespace std;
using namespace CVC4::kind;
using namespace CVC4::context;
using namespace CVC4::theory::inst;

namespace CVC4 {
namespace theory {
namespace quantifiers {

TermUtil::TermUtil(QuantifiersEngine * qe) :
d_quantEngine(qe),
d_op_id_count(0),
d_typ_id_count(0){
  d_true = NodeManager::currentNM()->mkConst(true);
  d_false = NodeManager::currentNM()->mkConst(false);
  d_zero = NodeManager::currentNM()->mkConst(Rational(0));
  d_one = NodeManager::currentNM()->mkConst(Rational(1));
}

TermUtil::~TermUtil(){

}

void TermUtil::registerQuantifier( Node q ){
  if( d_inst_constants.find( q )==d_inst_constants.end() ){
    Debug("quantifiers-engine") << "Instantiation constants for " << q << " : " << std::endl;
    for( unsigned i=0; i<q[0].getNumChildren(); i++ ){
      d_vars[q].push_back( q[0][i] );
      d_var_num[q][q[0][i]] = i;
      //make instantiation constants
      Node ic = NodeManager::currentNM()->mkInstConstant( q[0][i].getType() );
      d_inst_constants_map[ic] = q;
      d_inst_constants[ q ].push_back( ic );
      Debug("quantifiers-engine") << "  " << ic << std::endl;
      //set the var number attribute
      InstVarNumAttribute ivna;
      ic.setAttribute( ivna, i );
      InstConstantAttribute ica;
      ic.setAttribute( ica, q );
    }
  }
}

void TermUtil::getBoundVars2( Node n, std::vector< Node >& vars, std::map< Node, bool >& visited ) {
  if( visited.find( n )==visited.end() ){
    visited[n] = true;
    if( n.getKind()==BOUND_VARIABLE ){
      if( std::find( vars.begin(), vars.end(), n )==vars.end() ) {
        vars.push_back( n );
      }
    }
    for( unsigned i=0; i<n.getNumChildren(); i++ ){
      getBoundVars2( n[i], vars, visited );
    }
  }
}

Node TermUtil::getRemoveQuantifiers2( Node n, std::map< Node, Node >& visited ) {
  std::map< Node, Node >::iterator it = visited.find( n );
  if( it!=visited.end() ){
    return it->second;
  }else{
    Node ret = n;
    if( n.getKind()==FORALL ){
      ret = getRemoveQuantifiers2( n[1], visited );
    }else if( n.getNumChildren()>0 ){
      std::vector< Node > children;
      bool childrenChanged = false;
      for( unsigned i=0; i<n.getNumChildren(); i++ ){
        Node ni = getRemoveQuantifiers2( n[i], visited );
        childrenChanged = childrenChanged || ni!=n[i];
        children.push_back( ni );
      }
      if( childrenChanged ){
        if( n.getMetaKind() == kind::metakind::PARAMETERIZED ){
          children.insert( children.begin(), n.getOperator() );
        }
        ret = NodeManager::currentNM()->mkNode( n.getKind(), children );
      }
    }
    visited[n] = ret;
    return ret;
  }
}

Node TermUtil::getInstConstAttr( Node n ) {
  if (!n.hasAttribute(InstConstantAttribute()) ){
    Node q;
    for( unsigned i=0; i<n.getNumChildren(); i++ ){
      q = getInstConstAttr(n[i]);
      if( !q.isNull() ){
        break;
      }
    }
    InstConstantAttribute ica;
    n.setAttribute(ica, q);
  }
  return n.getAttribute(InstConstantAttribute());
}

bool TermUtil::hasInstConstAttr( Node n ) {
  return !getInstConstAttr(n).isNull();
}

Node TermUtil::getBoundVarAttr( Node n ) {
  if (!n.hasAttribute(BoundVarAttribute()) ){
    Node bv;
    if( n.getKind()==BOUND_VARIABLE ){
      bv = n;
    }else{
      for( unsigned i=0; i<n.getNumChildren(); i++ ){
        bv = getBoundVarAttr(n[i]);
        if( !bv.isNull() ){
          break;
        }
      }
    }
    BoundVarAttribute bva;
    n.setAttribute(bva, bv);
  }
  return n.getAttribute(BoundVarAttribute());
}

bool TermUtil::hasBoundVarAttr( Node n ) {
  return !getBoundVarAttr(n).isNull();
}

void TermUtil::getBoundVars( Node n, std::vector< Node >& vars ) {
  std::map< Node, bool > visited;
  return getBoundVars2( n, vars, visited );
}

//remove quantifiers
Node TermUtil::getRemoveQuantifiers( Node n ) {
  std::map< Node, Node > visited;
  return getRemoveQuantifiers2( n, visited );
}

//quantified simplify
Node TermUtil::getQuantSimplify( Node n ) {
  std::vector< Node > bvs;
  getBoundVars( n, bvs );
  if( bvs.empty() ) {
    return Rewriter::rewrite( n );
  }else{
    Node q = NodeManager::currentNM()->mkNode( FORALL, NodeManager::currentNM()->mkNode( BOUND_VAR_LIST, bvs ), n );
    q = Rewriter::rewrite( q );
    return getRemoveQuantifiers( q );
  }
}

/** get the i^th instantiation constant of q */
Node TermUtil::getInstantiationConstant( Node q, int i ) const {
  std::map< Node, std::vector< Node > >::const_iterator it = d_inst_constants.find( q );
  if( it!=d_inst_constants.end() ){
    return it->second[i];
  }else{
    return Node::null();
  }
}

/** get number of instantiation constants for q */
unsigned TermUtil::getNumInstantiationConstants( Node q ) const {
  std::map< Node, std::vector< Node > >::const_iterator it = d_inst_constants.find( q );
  if( it!=d_inst_constants.end() ){
    return it->second.size();
  }else{
    return 0;
  }
}

Node TermUtil::getInstConstantBody( Node q ){
  std::map< Node, Node >::iterator it = d_inst_const_body.find( q );
  if( it==d_inst_const_body.end() ){
    Node n = getInstConstantNode( q[1], q );
    d_inst_const_body[ q ] = n;
    return n;
  }else{
    return it->second;
  }
}

Node TermUtil::getCounterexampleLiteral( Node q ){
  if( d_ce_lit.find( q )==d_ce_lit.end() ){
    /*
    Node ceBody = getInstConstantBody( f );
    //check if any variable are of bad types, and fail if so
    for( size_t i=0; i<d_inst_constants[f].size(); i++ ){
      if( d_inst_constants[f][i].getType().isBoolean() ){
        d_ce_lit[ f ] = Node::null();
        return Node::null();
      }
    }
    */
    Node g = NodeManager::currentNM()->mkSkolem( "g", NodeManager::currentNM()->booleanType() );
    //otherwise, ensure literal
    Node ceLit = d_quantEngine->getValuation().ensureLiteral( g );
    d_ce_lit[ q ] = ceLit;
  }
  return d_ce_lit[ q ];
}

Node TermUtil::getInstConstantNode( Node n, Node q ){
  registerQuantifier( q );
  return n.substitute( d_vars[q].begin(), d_vars[q].end(), d_inst_constants[q].begin(), d_inst_constants[q].end() );
}

Node TermUtil::getVariableNode( Node n, Node q ) {
  registerQuantifier( q );
  return n.substitute( d_inst_constants[q].begin(), d_inst_constants[q].end(), d_vars[q].begin(), d_vars[q].end() );
}

Node TermUtil::getInstantiatedNode( Node n, Node q, std::vector< Node >& terms ) {
  Assert( d_vars[q].size()==terms.size() );
  return n.substitute( d_vars[q].begin(), d_vars[q].end(), terms.begin(), terms.end() );
}


void getSelfSel( const Datatype& dt, const DatatypeConstructor& dc, Node n, TypeNode ntn, std::vector< Node >& selfSel ){
  TypeNode tspec;
  if( dt.isParametric() ){
    tspec = TypeNode::fromType( dc.getSpecializedConstructorType(n.getType().toType()) );
    Trace("sk-ind-debug") << "Specialized constructor type : " << tspec << std::endl;
    Assert( tspec.getNumChildren()==dc.getNumArgs() );
  }
  Trace("sk-ind-debug") << "Check self sel " << dc.getName() << " " << dt.getName() << std::endl;
  for( unsigned j=0; j<dc.getNumArgs(); j++ ){
    std::vector< Node > ssc;
    if( dt.isParametric() ){
      Trace("sk-ind-debug") << "Compare " << tspec[j] << " " << ntn << std::endl;
      if( tspec[j]==ntn ){
        ssc.push_back( n );
      }
    }else{
      TypeNode tn = TypeNode::fromType( dc[j].getRangeType() );
      Trace("sk-ind-debug") << "Compare " << tn << " " << ntn << std::endl;
      if( tn==ntn ){
        ssc.push_back( n );
      }
    }
    /* TODO: more than weak structural induction
    else if( tn.isDatatype() && std::find( visited.begin(), visited.end(), tn )==visited.end() ){
      visited.push_back( tn );
      const Datatype& dt = ((DatatypeType)(subs[0].getType()).toType()).getDatatype();
      std::vector< Node > disj;
      for( unsigned i=0; i<dt.getNumConstructors(); i++ ){
        getSelfSel( dt[i], n, ntn, ssc );
      }
      visited.pop_back();
    }
    */
    for( unsigned k=0; k<ssc.size(); k++ ){
      Node ss = NodeManager::currentNM()->mkNode( APPLY_SELECTOR_TOTAL, dc.getSelectorInternal( n.getType().toType(), j ), n );
      if( std::find( selfSel.begin(), selfSel.end(), ss )==selfSel.end() ){
        selfSel.push_back( ss );
      }
    }
  }
}


Node TermUtil::mkSkolemizedBody( Node f, Node n, std::vector< TypeNode >& argTypes, std::vector< TNode >& fvs,
                               std::vector< Node >& sk, Node& sub, std::vector< unsigned >& sub_vars ) {
  Assert( sk.empty() || sk.size()==f[0].getNumChildren() );
  //calculate the variables and substitution
  std::vector< TNode > ind_vars;
  std::vector< unsigned > ind_var_indicies;
  std::vector< TNode > vars;
  std::vector< unsigned > var_indicies;
  for( unsigned i=0; i<f[0].getNumChildren(); i++ ){
    if( isInductionTerm( f[0][i] ) ){
      ind_vars.push_back( f[0][i] );
      ind_var_indicies.push_back( i );
    }else{
      vars.push_back( f[0][i] );
      var_indicies.push_back( i );
    }
    Node s;
    //make the new function symbol or use existing
    if( i>=sk.size() ){
      if( argTypes.empty() ){
        s = NodeManager::currentNM()->mkSkolem( "skv", f[0][i].getType(), "created during skolemization" );
      }else{
        TypeNode typ = NodeManager::currentNM()->mkFunctionType( argTypes, f[0][i].getType() );
        Node op = NodeManager::currentNM()->mkSkolem( "skop", typ, "op created during pre-skolemization" );
        //DOTHIS: set attribute on op, marking that it should not be selected as trigger
        std::vector< Node > funcArgs;
        funcArgs.push_back( op );
        funcArgs.insert( funcArgs.end(), fvs.begin(), fvs.end() );
        s = NodeManager::currentNM()->mkNode( kind::APPLY_UF, funcArgs );
      }
      sk.push_back( s );
    }else{
      Assert( sk[i].getType()==f[0][i].getType() );
    }
  }
  Node ret;
  if( vars.empty() ){
    ret = n;
  }else{
    std::vector< Node > var_sk;
    for( unsigned i=0; i<var_indicies.size(); i++ ){
      var_sk.push_back( sk[var_indicies[i]] );
    }
    Assert( vars.size()==var_sk.size() );
    ret = n.substitute( vars.begin(), vars.end(), var_sk.begin(), var_sk.end() );
  }
  if( !ind_vars.empty() ){
    Trace("sk-ind") << "Ind strengthen : (not " << f << ")" << std::endl;
    Trace("sk-ind") << "Skolemized is : " << ret << std::endl;
    Node n_str_ind;
    TypeNode tn = ind_vars[0].getType();
    Node k = sk[ind_var_indicies[0]];
    Node nret = ret.substitute( ind_vars[0], k );
    //note : everything is under a negation
    //the following constructs ~( R( x, k ) => ~P( x ) )
    if( options::dtStcInduction() && tn.isDatatype() ){
      const Datatype& dt = ((DatatypeType)(tn).toType()).getDatatype();
      std::vector< Node > disj;
      for( unsigned i=0; i<dt.getNumConstructors(); i++ ){
        std::vector< Node > selfSel;
        getSelfSel( dt, dt[i], k, tn, selfSel );
        std::vector< Node > conj;
        conj.push_back( NodeManager::currentNM()->mkNode( APPLY_TESTER, Node::fromExpr( dt[i].getTester() ), k ).negate() );
        for( unsigned j=0; j<selfSel.size(); j++ ){
          conj.push_back( ret.substitute( ind_vars[0], selfSel[j] ).negate() );
        }
        disj.push_back( conj.size()==1 ? conj[0] : NodeManager::currentNM()->mkNode( OR, conj ) );
      }
      Assert( !disj.empty() );
      n_str_ind = disj.size()==1 ? disj[0] : NodeManager::currentNM()->mkNode( AND, disj );
    }else if( options::intWfInduction() && tn.isInteger() ){
      Node icond = NodeManager::currentNM()->mkNode( GEQ, k, NodeManager::currentNM()->mkConst( Rational(0) ) );
      Node iret = ret.substitute( ind_vars[0], NodeManager::currentNM()->mkNode( MINUS, k, NodeManager::currentNM()->mkConst( Rational(1) ) ) ).negate();
      n_str_ind = NodeManager::currentNM()->mkNode( OR, icond.negate(), iret );
      n_str_ind = NodeManager::currentNM()->mkNode( AND, icond, n_str_ind );
    }else{
      Trace("sk-ind") << "Unknown induction for term : " << ind_vars[0] << ", type = " << tn << std::endl;
      Assert( false );
    }
    Trace("sk-ind") << "Strengthening is : " << n_str_ind << std::endl;

    std::vector< Node > rem_ind_vars;
    rem_ind_vars.insert( rem_ind_vars.end(), ind_vars.begin()+1, ind_vars.end() );
    if( !rem_ind_vars.empty() ){
      Node bvl = NodeManager::currentNM()->mkNode( BOUND_VAR_LIST, rem_ind_vars );
      nret = NodeManager::currentNM()->mkNode( FORALL, bvl, nret );
      nret = Rewriter::rewrite( nret );
      sub = nret;
      sub_vars.insert( sub_vars.end(), ind_var_indicies.begin()+1, ind_var_indicies.end() );
      n_str_ind = NodeManager::currentNM()->mkNode( FORALL, bvl, n_str_ind.negate() ).negate();
    }
    ret = NodeManager::currentNM()->mkNode( OR, nret, n_str_ind );
  }
  Trace("quantifiers-sk-debug") << "mkSkolem body for " << f << " returns : " << ret << std::endl;
  //if it has an instantiation level, set the skolemized body to that level
  if( f.hasAttribute(InstLevelAttribute()) ){
    theory::QuantifiersEngine::setInstantiationLevelAttr( ret, f.getAttribute(InstLevelAttribute()) );
  }

  if( Trace.isOn("quantifiers-sk") ){
    Trace("quantifiers-sk") << "Skolemize : ";
    for( unsigned i=0; i<sk.size(); i++ ){
      Trace("quantifiers-sk") << sk[i] << " ";
    }
    Trace("quantifiers-sk") << "for " << std::endl;
    Trace("quantifiers-sk") << "   " << f << std::endl;
  }

  return ret;
}

Node TermUtil::getSkolemizedBody( Node f ){
  Assert( f.getKind()==FORALL );
  if( d_skolem_body.find( f )==d_skolem_body.end() ){
    std::vector< TypeNode > fvTypes;
    std::vector< TNode > fvs;
    Node sub;
    std::vector< unsigned > sub_vars;
    d_skolem_body[ f ] = mkSkolemizedBody( f, f[1], fvTypes, fvs, d_skolem_constants[f], sub, sub_vars );
    //store sub quantifier information
    if( !sub.isNull() ){
      //if we are skolemizing one at a time, we already know the skolem constants of the sub-quantified formula, store them
      Assert( d_skolem_constants[sub].empty() );
      for( unsigned i=0; i<sub_vars.size(); i++ ){
        d_skolem_constants[sub].push_back( d_skolem_constants[f][sub_vars[i]] );
      }
    }
    Assert( d_skolem_constants[f].size()==f[0].getNumChildren() );
    if( options::sortInference() ){
      for( unsigned i=0; i<d_skolem_constants[f].size(); i++ ){
        //carry information for sort inference
        d_quantEngine->getTheoryEngine()->getSortInference()->setSkolemVar( f, f[0][i], d_skolem_constants[f][i] );
      }
    }
  }
  return d_skolem_body[ f ];
}

Node TermUtil::getEnumerateTerm( TypeNode tn, unsigned index ) {
  Trace("term-db-enum") << "Get enumerate term " << tn << " " << index << std::endl;
  std::map< TypeNode, unsigned >::iterator it = d_typ_enum_map.find( tn );
  unsigned teIndex;
  if( it==d_typ_enum_map.end() ){
    teIndex = (int)d_typ_enum.size();
    d_typ_enum_map[tn] = teIndex;
    d_typ_enum.push_back( TypeEnumerator(tn) );
  }else{
    teIndex = it->second;
  }
  while( index>=d_enum_terms[tn].size() ){
    if( d_typ_enum[teIndex].isFinished() ){
      return Node::null();
    }
    d_enum_terms[tn].push_back( *d_typ_enum[teIndex] );
    ++d_typ_enum[teIndex];
  }
  return d_enum_terms[tn][index];
}

bool TermUtil::isClosedEnumerableType( TypeNode tn ) {
  std::map< TypeNode, bool >::iterator it = d_typ_closed_enum.find( tn );
  if( it==d_typ_closed_enum.end() ){
    d_typ_closed_enum[tn] = true;
    bool ret = true;
    if( tn.isArray() || tn.isSort() || tn.isCodatatype() || tn.isFunction() ){
      ret = false;
    }else if( tn.isSet() ){
      ret = isClosedEnumerableType( tn.getSetElementType() );
    }else if( tn.isDatatype() ){
      const Datatype& dt = ((DatatypeType)(tn).toType()).getDatatype();
      for( unsigned i=0; i<dt.getNumConstructors(); i++ ){
        for( unsigned j=0; j<dt[i].getNumArgs(); j++ ){
          TypeNode ctn = TypeNode::fromType( dt[i][j].getRangeType() );
          if( tn!=ctn && !isClosedEnumerableType( ctn ) ){
            ret = false;
            break;
          }
        }
        if( !ret ){
          break;
        }
      }
    }
    //TODO: other parametric sorts go here
    d_typ_closed_enum[tn] = ret;
    return ret;
  }else{
    return it->second;
  }
}

//checks whether a type is not Array and is reasonably small enough (<1000) such that all of its domain elements can be enumerated
bool TermUtil::mayComplete( TypeNode tn ) {
  std::map< TypeNode, bool >::iterator it = d_may_complete.find( tn );
  if( it==d_may_complete.end() ){
    bool mc = false;
    if( isClosedEnumerableType( tn ) && tn.getCardinality().isFinite() && !tn.getCardinality().isLargeFinite() ){
      Node card = NodeManager::currentNM()->mkConst( Rational(tn.getCardinality().getFiniteCardinality()) );
      Node oth = NodeManager::currentNM()->mkConst( Rational(1000) );
      Node eq = NodeManager::currentNM()->mkNode( LEQ, card, oth );
      eq = Rewriter::rewrite( eq );
      mc = eq==d_true;
    }
    d_may_complete[tn] = mc;
    return mc;
  }else{
    return it->second;
  }
}

void TermUtil::computeVarContains( Node n, std::vector< Node >& varContains ) {
  std::map< Node, bool > visited;
  computeVarContains2( n, INST_CONSTANT, varContains, visited );
}

void TermUtil::computeQuantContains( Node n, std::vector< Node >& quantContains ) {
  std::map< Node, bool > visited;
  computeVarContains2( n, FORALL, quantContains, visited );
}


void TermUtil::computeVarContains2( Node n, Kind k, std::vector< Node >& varContains, std::map< Node, bool >& visited ){
  if( visited.find( n )==visited.end() ){
    visited[n] = true;
    if( n.getKind()==k ){
      if( std::find( varContains.begin(), varContains.end(), n )==varContains.end() ){
        varContains.push_back( n );
      }
    }else{
      for( unsigned i=0; i<n.getNumChildren(); i++ ){
        computeVarContains2( n[i], k, varContains, visited );
      }
    }
  }
}

void TermUtil::getVarContains( Node f, std::vector< Node >& pats, std::map< Node, std::vector< Node > >& varContains ){
  for( unsigned i=0; i<pats.size(); i++ ){
    varContains[ pats[i] ].clear();
    getVarContainsNode( f, pats[i], varContains[ pats[i] ] );
  }
}

void TermUtil::getVarContainsNode( Node f, Node n, std::vector< Node >& varContains ){
  std::vector< Node > vars;
  computeVarContains( n, vars );
  for( unsigned j=0; j<vars.size(); j++ ){
    Node v = vars[j];
    if( v.getAttribute(InstConstantAttribute())==f ){
      if( std::find( varContains.begin(), varContains.end(), v )==varContains.end() ){
        varContains.push_back( v );
      }
    }
  }
}

int TermUtil::isInstanceOf2( Node n1, Node n2, std::vector< Node >& varContains1, std::vector< Node >& varContains2 ){
  if( n1==n2 ){
    return 1;
  }else if( n1.getKind()==n2.getKind() ){
    if( n1.getKind()==APPLY_UF ){
      if( n1.getOperator()==n2.getOperator() ){
        int result = 0;
        for( int i=0; i<(int)n1.getNumChildren(); i++ ){
          if( n1[i]!=n2[i] ){
            int cResult = isInstanceOf2( n1[i], n2[i], varContains1, varContains2 );
            if( cResult==0 ){
              return 0;
            }else if( cResult!=result ){
              if( result!=0 ){
                return 0;
              }else{
                result = cResult;
              }
            }
          }
        }
        return result;
      }
    }
    return 0;
  }else if( n2.getKind()==INST_CONSTANT ){
    //if( std::find( d_var_contains[ n1 ].begin(), d_var_contains[ n1 ].end(), n2 )!=d_var_contains[ n1 ].end() ){
    //  return 1;
    //}
    if( varContains1.size()==1 && varContains1[ 0 ]==n2 ){
      return 1;
    }
  }else if( n1.getKind()==INST_CONSTANT ){
    //if( std::find( d_var_contains[ n2 ].begin(), d_var_contains[ n2 ].end(), n1 )!=d_var_contains[ n2 ].end() ){
    //  return -1;
    //}
    if( varContains2.size()==1 && varContains2[ 0 ]==n1 ){
      return 1;
    }
  }
  return 0;
}

int TermUtil::isInstanceOf( Node n1, Node n2 ){
  std::vector< Node > varContains1;
  std::vector< Node > varContains2;
  computeVarContains( n1, varContains1 );
  computeVarContains( n1, varContains2 );
  return isInstanceOf2( n1, n2, varContains1, varContains2 );
}

bool TermUtil::isUnifiableInstanceOf( Node n1, Node n2, std::map< Node, Node >& subs ){
  if( n1==n2 ){
    return true;
  }else if( n2.getKind()==INST_CONSTANT ){
    //if( !node_contains( n1, n2 ) ){
    //  return false;
    //}
    if( subs.find( n2 )==subs.end() ){
      subs[n2] = n1;
    }else if( subs[n2]!=n1 ){
      return false;
    }
    return true;
  }else if( n1.getKind()==n2.getKind() && n1.getMetaKind()==kind::metakind::PARAMETERIZED ){
    if( n1.getOperator()!=n2.getOperator() ){
      return false;
    }
    for( int i=0; i<(int)n1.getNumChildren(); i++ ){
      if( !isUnifiableInstanceOf( n1[i], n2[i], subs ) ){
        return false;
      }
    }
    return true;
  }else{
    return false;
  }
}

void TermUtil::filterInstances( std::vector< Node >& nodes ){
  std::vector< bool > active;
  active.resize( nodes.size(), true );
  std::map< int, std::vector< Node > > varContains;
  for( unsigned i=0; i<nodes.size(); i++ ){
    computeVarContains( nodes[i], varContains[i] );
  }
  for( unsigned i=0; i<nodes.size(); i++ ){
    for( unsigned j=i+1; j<nodes.size(); j++ ){
      if( active[i] && active[j] ){
        int result = isInstanceOf2( nodes[i], nodes[j], varContains[i], varContains[j] );
        if( result==1 ){
          Trace("filter-instances") << nodes[j] << " is an instance of " << nodes[i] << std::endl;
          active[j] = false;
        }else if( result==-1 ){
          Trace("filter-instances") << nodes[i] << " is an instance of " << nodes[j] << std::endl;
          active[i] = false;
        }
      }
    }
  }
  std::vector< Node > temp;
  for( unsigned i=0; i<nodes.size(); i++ ){
    if( active[i] ){
      temp.push_back( nodes[i] );
    }
  }
  nodes.clear();
  nodes.insert( nodes.begin(), temp.begin(), temp.end() );
}

int TermUtil::getIdForOperator( Node op ) {
  std::map< Node, int >::iterator it = d_op_id.find( op );
  if( it==d_op_id.end() ){
    d_op_id[op] = d_op_id_count;
    d_op_id_count++;
    return d_op_id[op];
  }else{
    return it->second;
  }
}

int TermUtil::getIdForType( TypeNode t ) {
  std::map< TypeNode, int >::iterator it = d_typ_id.find( t );
  if( it==d_typ_id.end() ){
    d_typ_id[t] = d_typ_id_count;
    d_typ_id_count++;
    return d_typ_id[t];
  }else{
    return it->second;
  }
}

bool TermUtil::getTermOrder( Node a, Node b ) {
  if( a.getKind()==BOUND_VARIABLE ){
    if( b.getKind()==BOUND_VARIABLE ){
      return a.getAttribute(InstVarNumAttribute())<b.getAttribute(InstVarNumAttribute());
    }else{
      return true;
    }
  }else if( b.getKind()!=BOUND_VARIABLE ){
    Node aop = a.hasOperator() ? a.getOperator() : a;
    Node bop = b.hasOperator() ? b.getOperator() : b;
    Trace("aeq-debug2") << a << "...op..." << aop << std::endl;
    Trace("aeq-debug2") << b << "...op..." << bop << std::endl;
    if( aop==bop ){
      if( a.getNumChildren()==b.getNumChildren() ){
        for( unsigned i=0; i<a.getNumChildren(); i++ ){
          if( a[i]!=b[i] ){
            //first distinct child determines the ordering
            return getTermOrder( a[i], b[i] );
          }
        }
      }else{
        return aop.getNumChildren()<bop.getNumChildren();
      }
    }else{
      return getIdForOperator( aop )<getIdForOperator( bop );
    }
  }
  return false;
}



Node TermUtil::getCanonicalFreeVar( TypeNode tn, unsigned i ) {
  Assert( !tn.isNull() );
  while( d_cn_free_var[tn].size()<=i ){
    std::stringstream oss;
    oss << tn;
    std::string typ_name = oss.str();
    while( typ_name[0]=='(' ){
      typ_name.erase( typ_name.begin() );
    }
    std::stringstream os;
    os << typ_name[0] << i;
    Node x = NodeManager::currentNM()->mkBoundVar( os.str().c_str(), tn );
    InstVarNumAttribute ivna;
    x.setAttribute(ivna,d_cn_free_var[tn].size());
    d_cn_free_var[tn].push_back( x );
  }
  return d_cn_free_var[tn][i];
}

struct sortTermOrder {
  TermUtil* d_tu;
  //std::map< Node, std::map< Node, bool > > d_cache;
  bool operator() (Node i, Node j) {
    /*
    //must consult cache since term order is partial?
    std::map< Node, bool >::iterator it = d_cache[j].find( i );
    if( it!=d_cache[j].end() && it->second ){
      return false;
    }else{
      bool ret = d_tdb->getTermOrder( i, j );
      d_cache[i][j] = ret;
      return ret;
    }
    */
    return d_tu->getTermOrder( i, j );
  }
};

//this function makes a canonical representation of a term (
//  - orders variables left to right
//  - if apply_torder, then sort direct subterms of commutative operators
Node TermUtil::getCanonicalTerm( TNode n, std::map< TypeNode, unsigned >& var_count, std::map< TNode, TNode >& subs, bool apply_torder, std::map< TNode, Node >& visited ) {
  Trace("canon-term-debug") << "Get canonical term for " << n << std::endl;
  if( n.getKind()==BOUND_VARIABLE ){
    std::map< TNode, TNode >::iterator it = subs.find( n );
    if( it==subs.end() ){
      TypeNode tn = n.getType();
      //allocate variable
      unsigned vn = var_count[tn];
      var_count[tn]++;
      subs[n] = getCanonicalFreeVar( tn, vn );
      Trace("canon-term-debug") << "...allocate variable." << std::endl;
      return subs[n];
    }else{
      Trace("canon-term-debug") << "...return variable in subs." << std::endl;
      return it->second;
    }
  }else if( n.getNumChildren()>0 ){
    std::map< TNode, Node >::iterator it = visited.find( n );
    if( it!=visited.end() ){
      return it->second;
    }else{
      //collect children
      Trace("canon-term-debug") << "Collect children" << std::endl;
      std::vector< Node > cchildren;
      for( unsigned i=0; i<n.getNumChildren(); i++ ){
        cchildren.push_back( n[i] );
      }
      //if applicable, first sort by term order
      if( apply_torder && isComm( n.getKind() ) ){
        Trace("canon-term-debug") << "Sort based on commutative operator " << n.getKind() << std::endl;
        sortTermOrder sto;
        sto.d_tu = this;
        std::sort( cchildren.begin(), cchildren.end(), sto );
      }
      //now make canonical
      Trace("canon-term-debug") << "Make canonical children" << std::endl;
      for( unsigned i=0; i<cchildren.size(); i++ ){
        cchildren[i] = getCanonicalTerm( cchildren[i], var_count, subs, apply_torder, visited );
      }
      if( n.getMetaKind() == kind::metakind::PARAMETERIZED ){
        Trace("canon-term-debug") << "Insert operator " << n.getOperator() << std::endl;
        cchildren.insert( cchildren.begin(), n.getOperator() );
      }
      Trace("canon-term-debug") << "...constructing for " << n << "." << std::endl;
      Node ret = NodeManager::currentNM()->mkNode( n.getKind(), cchildren );
      Trace("canon-term-debug") << "...constructed " << ret << " for " << n << "." << std::endl;
      visited[n] = ret;
      return ret;
    }
  }else{
    Trace("canon-term-debug") << "...return 0-child term." << std::endl;
    return n;
  }
}

Node TermUtil::getCanonicalTerm( TNode n, bool apply_torder ){
  std::map< TypeNode, unsigned > var_count;
  std::map< TNode, TNode > subs;
  std::map< TNode, Node > visited;
  return getCanonicalTerm( n, var_count, subs, apply_torder, visited );
}

void TermUtil::getVtsTerms( std::vector< Node >& t, bool isFree, bool create, bool inc_delta ) {
  if( inc_delta ){
    Node delta = getVtsDelta( isFree, create );
    if( !delta.isNull() ){
      t.push_back( delta );
    }
  }
  for( unsigned r=0; r<2; r++ ){
    Node inf = getVtsInfinityIndex( r, isFree, create );
    if( !inf.isNull() ){
      t.push_back( inf );
    }
  }
}

Node TermUtil::getVtsDelta( bool isFree, bool create ) {
  if( create ){
    if( d_vts_delta_free.isNull() ){
      d_vts_delta_free = NodeManager::currentNM()->mkSkolem( "delta_free", NodeManager::currentNM()->realType(), "free delta for virtual term substitution" );
      Node delta_lem = NodeManager::currentNM()->mkNode( GT, d_vts_delta_free, d_zero );
      d_quantEngine->getOutputChannel().lemma( delta_lem );
    }
    if( d_vts_delta.isNull() ){
      d_vts_delta = NodeManager::currentNM()->mkSkolem( "delta", NodeManager::currentNM()->realType(), "delta for virtual term substitution" );
      //mark as a virtual term
      VirtualTermSkolemAttribute vtsa;
      d_vts_delta.setAttribute(vtsa,true);
    }
  }
  return isFree ? d_vts_delta_free : d_vts_delta;
}

Node TermUtil::getVtsInfinity( TypeNode tn, bool isFree, bool create ) {
  if( create ){
    if( d_vts_inf_free[tn].isNull() ){
      d_vts_inf_free[tn] = NodeManager::currentNM()->mkSkolem( "inf_free", tn, "free infinity for virtual term substitution" );
    }
    if( d_vts_inf[tn].isNull() ){
      d_vts_inf[tn] = NodeManager::currentNM()->mkSkolem( "inf", tn, "infinity for virtual term substitution" );
      //mark as a virtual term
      VirtualTermSkolemAttribute vtsa;
      d_vts_inf[tn].setAttribute(vtsa,true);
    }
  }
  return isFree ? d_vts_inf_free[tn] : d_vts_inf[tn];
}

Node TermUtil::getVtsInfinityIndex( int i, bool isFree, bool create ) {
  if( i==0 ){
    return getVtsInfinity( NodeManager::currentNM()->realType(), isFree, create );
  }else if( i==1 ){
    return getVtsInfinity( NodeManager::currentNM()->integerType(), isFree, create );
  }else{
    Assert( false );
    return Node::null();
  }
}

Node TermUtil::substituteVtsFreeTerms( Node n ) {
  std::vector< Node > vars;
  getVtsTerms( vars, false, false );
  std::vector< Node > vars_free;
  getVtsTerms( vars_free, true, false );
  Assert( vars.size()==vars_free.size() );
  if( !vars.empty() ){
    return n.substitute( vars.begin(), vars.end(), vars_free.begin(), vars_free.end() );
  }else{
    return n;
  }
}

Node TermUtil::rewriteVtsSymbols( Node n ) {
  if( ( n.getKind()==EQUAL || n.getKind()==GEQ ) ){
    Trace("quant-vts-debug") << "VTS : process " << n << std::endl;
    Node rew_vts_inf;
    bool rew_delta = false;
    //rewriting infinity always takes precedence over rewriting delta
    for( unsigned r=0; r<2; r++ ){
      Node inf = getVtsInfinityIndex( r, false, false );
      if( !inf.isNull() && containsTerm( n, inf ) ){
        if( rew_vts_inf.isNull() ){
          rew_vts_inf = inf;
        }else{
          //for mixed int/real with multiple infinities
          Trace("quant-vts-debug") << "Multiple infinities...equate " << inf << " = " << rew_vts_inf << std::endl;
          std::vector< Node > subs_lhs;
          subs_lhs.push_back( inf );
          std::vector< Node > subs_rhs;
          subs_lhs.push_back( rew_vts_inf );
          n = n.substitute( subs_lhs.begin(), subs_lhs.end(), subs_rhs.begin(), subs_rhs.end() );
          n = Rewriter::rewrite( n );
          //may have cancelled
          if( !containsTerm( n, rew_vts_inf ) ){
            rew_vts_inf = Node::null();
          }
        }
      }
    }
    if( rew_vts_inf.isNull() ){
      if( !d_vts_delta.isNull() && containsTerm( n, d_vts_delta ) ){
        rew_delta = true;
      }
    }
    if( !rew_vts_inf.isNull()  || rew_delta ){
      std::map< Node, Node > msum;
      if( QuantArith::getMonomialSumLit( n, msum ) ){
        if( Trace.isOn("quant-vts-debug") ){
          Trace("quant-vts-debug") << "VTS got monomial sum : " << std::endl;
          QuantArith::debugPrintMonomialSum( msum, "quant-vts-debug" );
        }
        Node vts_sym = !rew_vts_inf.isNull() ? rew_vts_inf : d_vts_delta;
        Assert( !vts_sym.isNull() );
        Node iso_n;
        Node nlit;
        int res = QuantArith::isolate( vts_sym, msum, iso_n, n.getKind(), true );
        if( res!=0 ){
          Trace("quant-vts-debug") << "VTS isolated :  -> " << iso_n << ", res = " << res << std::endl;
          Node slv = iso_n[res==1 ? 1 : 0];
          //ensure the vts terms have been eliminated
          if( containsVtsTerm( slv ) ){
            Trace("quant-vts-warn") << "Bad vts literal : " << n << ", contains " << vts_sym << " but bad solved form " << slv << "." << std::endl;
            nlit = substituteVtsFreeTerms( n );
            Trace("quant-vts-debug") << "...return " << nlit << std::endl;
            //Assert( false );
            //safe case: just convert to free symbols
            return nlit;
          }else{
            if( !rew_vts_inf.isNull() ){
              nlit = ( n.getKind()==GEQ && res==1 ) ? d_true : d_false;
            }else{
              Assert( iso_n[res==1 ? 0 : 1]==d_vts_delta );
              if( n.getKind()==EQUAL ){
                nlit = d_false;
              }else if( res==1 ){
                nlit = NodeManager::currentNM()->mkNode( GEQ, d_zero, slv );
              }else{
                nlit = NodeManager::currentNM()->mkNode( GT, slv, d_zero );
              }
            }
          }
          Trace("quant-vts-debug") << "Return " << nlit << std::endl;
          return nlit;
        }else{
          Trace("quant-vts-warn") << "Bad vts literal : " << n << ", contains " << vts_sym << " but could not isolate." << std::endl;
          //safe case: just convert to free symbols
          nlit = substituteVtsFreeTerms( n );
          Trace("quant-vts-debug") << "...return " << nlit << std::endl;
          //Assert( false );
          return nlit;
        }
      }
    }
    return n;
  }else if( n.getKind()==FORALL ){
    //cannot traverse beneath quantifiers
    return substituteVtsFreeTerms( n );
  }else{
    bool childChanged = false;
    std::vector< Node > children;
    for( unsigned i=0; i<n.getNumChildren(); i++ ){
      Node nn = rewriteVtsSymbols( n[i] );
      children.push_back( nn );
      childChanged = childChanged || nn!=n[i];
    }
    if( childChanged ){
      if( n.getMetaKind() == kind::metakind::PARAMETERIZED ){
        children.insert( children.begin(), n.getOperator() );
      }
      Node ret = NodeManager::currentNM()->mkNode( n.getKind(), children );
      Trace("quant-vts-debug") << "...make node " << ret << std::endl;
      return ret;
    }else{
      return n;
    }
  }
}

bool TermUtil::containsVtsTerm( Node n, bool isFree ) {
  std::vector< Node > t;
  getVtsTerms( t, isFree, false );
  return containsTerms( n, t );
}

bool TermUtil::containsVtsTerm( std::vector< Node >& n, bool isFree ) {
  std::vector< Node > t;
  getVtsTerms( t, isFree, false );
  if( !t.empty() ){
    for( unsigned i=0; i<n.size(); i++ ){
      if( containsTerms( n[i], t ) ){
        return true;
      }
    }
  }
  return false;
}

bool TermUtil::containsVtsInfinity( Node n, bool isFree ) {
  std::vector< Node > t;
  getVtsTerms( t, isFree, false, false );
  return containsTerms( n, t );
}

Node TermUtil::ensureType( Node n, TypeNode tn ) {
  TypeNode ntn = n.getType();
  Assert( ntn.isComparableTo( tn ) );
  if( ntn.isSubtypeOf( tn ) ){
    return n;
  }else{
    if( tn.isInteger() ){
      return NodeManager::currentNM()->mkNode( TO_INTEGER, n );
    }
    return Node::null();
  }
}

void TermUtil::getRelevancyCondition( Node n, std::vector< Node >& cond ) {
  if( n.getKind()==APPLY_SELECTOR_TOTAL ){
    // don't worry about relevancy conditions if using shared selectors
    if( !options::dtSharedSelectors() ){
      unsigned scindex = Datatype::cindexOf(n.getOperator().toExpr());
      const Datatype& dt = ((DatatypeType)(n[0].getType()).toType()).getDatatype();
      Node rc = NodeManager::currentNM()->mkNode( APPLY_TESTER, Node::fromExpr( dt[scindex].getTester() ), n[0] ).negate();
      if( std::find( cond.begin(), cond.end(), rc )==cond.end() ){
        cond.push_back( rc );
      }
      getRelevancyCondition( n[0], cond );
    }
  }
}

bool TermUtil::containsTerm2( Node n, Node t, std::map< Node, bool >& visited ) {
  if( n==t ){
    return true;
  }else{
    if( visited.find( n )==visited.end() ){
      visited[n] = true;
      for( unsigned i=0; i<n.getNumChildren(); i++ ){
        if( containsTerm2( n[i], t, visited ) ){
          return true;
        }
      }
    }
    return false;
  }
}

bool TermUtil::containsTerms2( Node n, std::vector< Node >& t, std::map< Node, bool >& visited ) {
  if( visited.find( n )==visited.end() ){
    if( std::find( t.begin(), t.end(), n )!=t.end() ){
      return true;
    }else{
      visited[n] = true;
      for( unsigned i=0; i<n.getNumChildren(); i++ ){
        if( containsTerms2( n[i], t, visited ) ){
          return true;
        }
      }
    }
  }
  return false;
}

bool TermUtil::containsTerm( Node n, Node t ) {
  std::map< Node, bool > visited;
  return containsTerm2( n, t, visited );
}

bool TermUtil::containsTerms( Node n, std::vector< Node >& t ) {
  if( t.empty() ){
    return false;
  }else{
    std::map< Node, bool > visited;
    return containsTerms2( n, t, visited );
  }
}

int TermUtil::getTermDepth( Node n ) {
  if (!n.hasAttribute(TermDepthAttribute()) ){
    int maxDepth = -1;
    for( unsigned i=0; i<n.getNumChildren(); i++ ){
      int depth = getTermDepth( n[i] );
      if( depth>maxDepth ){
        maxDepth = depth;
      }
    }
    TermDepthAttribute tda;
    n.setAttribute(tda,1+maxDepth);
  }
  return n.getAttribute(TermDepthAttribute());
}

bool TermUtil::containsUninterpretedConstant( Node n ) {
  if (!n.hasAttribute(ContainsUConstAttribute()) ){
    bool ret = false;
    if( n.getKind()==UNINTERPRETED_CONSTANT ){
      ret = true;
    }else{ 
      for( unsigned i=0; i<n.getNumChildren(); i++ ){
        if( containsUninterpretedConstant( n[i] ) ){
          ret = true;
          break;
        }
      }
    }
    ContainsUConstAttribute cuca;
    n.setAttribute(cuca, ret ? 1 : 0);
  }
  return n.getAttribute(ContainsUConstAttribute())!=0;
}

Node TermUtil::simpleNegate( Node n ){
  if( n.getKind()==OR || n.getKind()==AND ){
    std::vector< Node > children;
    for( unsigned i=0; i<n.getNumChildren(); i++ ){
      children.push_back( simpleNegate( n[i] ) );
    }
    return NodeManager::currentNM()->mkNode( n.getKind()==OR ? AND : OR, children );
  }else{
    return n.negate();
  }
}

bool TermUtil::isAssoc( Kind k ) {
  return k==PLUS || k==MULT || k==AND || k==OR || 
         k==BITVECTOR_PLUS || k==BITVECTOR_MULT || k==BITVECTOR_AND || k==BITVECTOR_OR || k==BITVECTOR_XOR || k==BITVECTOR_XNOR || k==BITVECTOR_CONCAT ||
         k==STRING_CONCAT;
}

bool TermUtil::isComm( Kind k ) {
  return k==EQUAL || k==PLUS || k==MULT || k==AND || k==OR || k==XOR || 
         k==BITVECTOR_PLUS || k==BITVECTOR_MULT || k==BITVECTOR_AND || k==BITVECTOR_OR || k==BITVECTOR_XOR || k==BITVECTOR_XNOR;
}

bool TermUtil::isNonAdditive( Kind k ) {
  return k==AND || k==OR || k==BITVECTOR_AND || k==BITVECTOR_OR;
}

bool TermUtil::isBoolConnective( Kind k ) {
  return k==OR || k==AND || k==EQUAL || k==ITE || k==FORALL || k==NOT || k==SEP_STAR;
}

bool TermUtil::isBoolConnectiveTerm( TNode n ) {
  return isBoolConnective( n.getKind() ) &&
         ( n.getKind()!=EQUAL || n[0].getType().isBoolean() ) && 
         ( n.getKind()!=ITE || n.getType().isBoolean() );
}

void TermUtil::registerTrigger( theory::inst::Trigger* tr, Node op ){
  if( std::find( d_op_triggers[op].begin(), d_op_triggers[op].end(), tr )==d_op_triggers[op].end() ){
    d_op_triggers[op].push_back( tr );
  }
}

Node TermUtil::getHoTypeMatchPredicate( TypeNode tn ) {
  std::map< TypeNode, Node >::iterator ithp = d_ho_type_match_pred.find( tn );
  if( ithp==d_ho_type_match_pred.end() ){
    TypeNode ptn = NodeManager::currentNM()->mkFunctionType( tn, NodeManager::currentNM()->booleanType() );
    Node k = NodeManager::currentNM()->mkSkolem( "U", ptn, "predicate to force higher-order types" );
    d_ho_type_match_pred[tn] = k;
    return k;
  }else{
    return ithp->second;  
  }
}

bool TermUtil::isInductionTerm( Node n ) {
  TypeNode tn = n.getType();
  if( options::dtStcInduction() && tn.isDatatype() ){
    const Datatype& dt = ((DatatypeType)(tn).toType()).getDatatype();
    return !dt.isCodatatype();
  }
  if( options::intWfInduction() && n.getType().isInteger() ){
    return true;
  }
  return false;
}

}/* CVC4::theory::quantifiers namespace */
}/* CVC4::theory namespace */
}/* CVC4 namespace */
