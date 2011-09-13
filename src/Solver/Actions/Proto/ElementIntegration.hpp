// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_Solver_Actions_Proto_ElementIntegration_hpp
#define CF_Solver_Actions_Proto_ElementIntegration_hpp

#include <boost/mpl/assert.hpp>

#include <boost/proto/transform/lazy.hpp>

#include "Mesh/Integrators/Gauss.hpp"

#include "ElementMatrix.hpp"
#include "ElementTransforms.hpp"
#include "IndexLooping.hpp"

/// @file 
/// Transforms used in element-wise expression evaluation

namespace CF {
namespace Solver {
namespace Actions {
namespace Proto {

/// Tag for an integral, wit the order provided as an MPL integral constant
template<typename OrderT>
struct IntegralTag
{
};

template<Uint Order, typename ExprT>
inline typename boost::proto::result_of::make_expr< boost::proto::tag::function, IntegralTag< boost::mpl::int_<Order> >, ExprT const & >::type const
integral(ExprT const & expr)
{
  return boost::proto::make_expr<boost::proto::tag::function>( IntegralTag< boost::mpl::int_<Order> >(), boost::ref(expr) );
}

/// Primitive transform that evaluates an integral using the Gauss points and returns the result as a reference to a stored matrix (or scalar)
struct GaussIntegral :
  boost::proto::transform< GaussIntegral >
{
/// Helper structs
private:
  /// Helper to extract the integration order
  template<typename T>
  struct GetOrder;
  
  template<int I>
  struct GetOrder< IntegralTag< boost::mpl::int_<I> > >
  {
    static const int value = I;
  };
  
public:
    
  template<typename ExprT, typename StateT, typename DataT>
  struct impl : boost::proto::transform_impl<ExprT, StateT, DataT>
  {
    /// Mapped coordinate type to use is obtained from the support
    typedef typename boost::remove_reference<DataT>::type::SupportT::EtypeT ShapeFunctionT;
    typedef typename ShapeFunctionT::MappedCoordsT MappedCoordsT;
    
    /// function argument 0 contains the terminal representing the function, and thus also the integration order
    typedef typename boost::remove_const
    <
      typename boost::remove_reference
      <
        typename boost::proto::result_of::value
        <
          typename boost::proto::result_of::child_c<ExprT, 0>::type
        >::type
      >::type
    >::type IntegralTagT;
    
    // Whew, we got the order
    static const int order = GetOrder<IntegralTagT>::value;
    
    typedef typename boost::proto::result_of::child_c<ExprT, 1>::type ChildT;
    
    // Basic result of the expression to integrate might be an Eigen expression
    typedef typename boost::remove_const
    <
      typename boost::remove_reference
      <
        typename boost::result_of
        <
          ElementMathImplicit
          (
            ChildT,
            const MappedCoordsT&,
            DataT
          )
        >::type
      >::type
    >::type EigenExprT;
    
    // Converter to get a real matrix type that can hold the result
    typedef ValueType
    <
      EigenExprT
    > ValueT;
    
    typedef const typename ValueT::type& result_type;
    
    result_type operator ()(typename impl::expr_param expr, typename impl::state_param state, typename impl::data_param data) const
    {
      typedef Mesh::Integrators::GaussMappedCoords<order, ShapeFunctionT::shape> GaussT;
      ChildT e = boost::proto::child_c<1>(expr); // expression to integrate
      data.precompute_element_matrices(GaussT::instance().coords.col(0), expr);
      expr.value = GaussT::instance().weights[0] * ElementMathImplicit()(e, state, data);
      for(Uint i = 1; i != GaussT::nb_points; ++i)
      {
        data.precompute_element_matrices(GaussT::instance().coords.col(i), expr);
        expr.value += GaussT::instance().weights[i] * ElementMathImplicit()(e, state, data);
      }
      return expr.value;
    }
    
  };  
};

template<typename GrammarT>
struct ElementQuadrature :
  boost::proto::transform< ElementQuadrature<GrammarT> >
{
  template<typename ExprT, typename StateT, typename DataT>
  struct impl : boost::proto::transform_impl<ExprT, StateT, DataT>
  { 
    
    typedef void result_type;
  
    /// Fusion functor to evaluate each child expression using the GrammarT supplied in the template argument
    struct evaluate_expr
    {
      evaluate_expr(typename impl::state_param state, typename impl::data_param data, const Real weight) :
        m_state(state),
        m_data(data),
        m_weight(weight * data.support().jacobian_determinant())
      {
      }
      
      template<typename ChildExprT>
      void operator()(ChildExprT& expr) const
      {
        tag_dispatch(typename boost::proto::tag_of<ChildExprT>::type(), expr);
      }
      
      template<typename ChildExprT>
      void tag_dispatch(const boost::proto::tag::plus_assign, ChildExprT& expr) const
      {
        GrammarT()(boost::proto::left(expr) += m_weight * boost::proto::right(expr), m_state, m_data);
      }
      
      // Issue an error message if a tag was not supported
      template<typename TagT, typename ChildExprT>
      void tag_dispatch(const TagT, ChildExprT& expr) const
      {
        BOOST_MPL_ASSERT(( boost::is_same<TagT, boost::proto::tag::plus_assign> ));
      }

    private:
      typename impl::state_param  m_state;
      typename impl::data_param m_data;
      const Real m_weight; // The integration weight (gauss point weight * jacobian determinant)
    };
    
    void operator ()(
                typename impl::expr_param expr
              , typename impl::state_param state
              , typename impl::data_param data
    ) const
    {
      typedef typename boost::remove_reference<DataT>::type::SupportT::EtypeT ShapeFunctionT;
      typedef typename ShapeFunctionT::MappedCoordsT MappedCoordsT;
      typedef Mesh::Integrators::GaussMappedCoords<2, ShapeFunctionT::shape> GaussT;
      
      for(Uint i = 0; i != GaussT::nb_points; ++i)
      {
        // Precompute the primitive element matrices (shape function values, gradients, ...) for the current Gauss point
        data.precompute_element_matrices(GaussT::instance().coords.col(i), expr);
        tag_dispatch(typename boost::proto::tag_of<ExprT>::type(), expr, state, data, GaussT::instance().weights[i]);
      }
    }
    
    /// Choose based on tag,element_quadrature << (expr1, expr2, ..., exprN) syntax
    void tag_dispatch(const boost::proto::tag::shift_left,
                      typename impl::expr_param expr,
                      typename impl::state_param state,
                      typename impl::data_param data,
                      const Real weight) const
    {
      boost::fusion::for_each(boost::proto::flatten(boost::proto::right(expr)), evaluate_expr(state, data, weight) );
    }
    
    /// Choose based on tag, element_quadrature(expr) syntax
    void tag_dispatch(const boost::proto::tag::function,
                      typename impl::expr_param expr,
                      typename impl::state_param state,
                      typename impl::data_param data,
                      const Real weight) const
    {
      evaluate_expr(state, data, weight)(boost::proto::child_c<1>(expr));
    }
  };
};

/// Use group(expr1, expr2, ..., exprN) to evaluate a group of expressions
static boost::proto::terminal<ElementQuadratureTag>::type element_quadrature = {};


/// Grammar that allows looping over I and J indices
template<typename I, typename J>
struct ElementMathImplicitIndexed :
  boost::proto::or_
  <
    SFOps< boost::proto::call< ElementMathImplicitIndexed<I, J> > >,
    FieldInterpolation,
    MathTerminals,
    ElementMatrixGrammar,
    ElementMatrixGrammarIndexed<I, J>,
    EigenMath<boost::proto::call< ElementMathImplicitIndexed<I, J> >, boost::proto::or_< Integers, IndexValues<I, J> > >
  >
{
};

struct ElementIntegration :
  boost::proto::or_
  <
    boost::proto::when
    <
      boost::proto::function< boost::proto::terminal< IntegralTag<boost::proto::_> >, ElementMathImplicit >,
      GaussIntegral
    >,
    boost::proto::when // function call syntax, to integrate a single += expression
    <
      boost::proto::function< boost::proto::terminal<ElementQuadratureTag>, boost::proto::_ >,
      ElementQuadrature< boost::proto::call< IndexLooper<ElementMathImplicitIndexed> > >
    >,
    boost::proto::when // << syntax, to integrate any number of += expressions
    <
      boost::proto::shift_left< boost::proto::terminal<ElementQuadratureTag>, boost::proto::comma<boost::proto::_, boost::proto::_> >,
      ElementQuadrature< boost::proto::call< IndexLooper<ElementMathImplicitIndexed> > >
    >
  >
{
};

  
} // namespace Proto
} // namespace Actions
} // namespace Solver
} // namespace CF

#endif // CF_Solver_Actions_Proto_ElementIntegration_hpp
