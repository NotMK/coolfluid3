// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_Solver_Actions_Proto_Expression_hpp
#define CF_Solver_Actions_Proto_Expression_hpp

#include <map>

#include <boost/mpl/for_each.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>

#include "Common/FindComponents.hpp"
#include "Common/Foreach.hpp"
#include "Common/Option.hpp"
#include "Common/OptionT.hpp"
#include "Common/OptionList.hpp"

#include "Math/VariableManager.hpp"
#include "Math/VariablesDescriptor.hpp"

#include "Mesh/CRegion.hpp"
#include "Mesh/CElements.hpp"
#include "Mesh/LagrangeP1/ElementTypes.hpp"
#include "Physics/PhysModel.hpp"

#include "ConfigurableConstant.hpp"
#include "ElementLooper.hpp"
#include "ElementMatrix.hpp"
#include "NodeLooper.hpp"
#include "NodeGrammar.hpp"
#include "Transforms.hpp"

namespace CF {
namespace Solver {
namespace Actions {
namespace Proto {

/// Abstract interface for classes that can hold a proto expression
class Expression
{
public:
  /// Pointer typedefs
  typedef boost::shared_ptr<Expression> Ptr;
  typedef boost::shared_ptr<Expression const> ConstPtr;

  /// Run the stored expression in a loop over the region
  virtual void loop(Mesh::CRegion& region) = 0;

  /// Generate the required options for configurable items in the expression
  /// If an option already existed, only a link will be created
  /// @param options The optionlist that will hold the generated options
  virtual void add_options(Common::OptionList& options) = 0;

  /// Register the variables that appear in the expression with a physical model
  virtual void register_variables(Physics::PhysModel& physical_model) = 0;

  virtual ~Expression() {}
};

/// Boilerplate implementation
template<typename ExprT>
class ExpressionBase : public Expression
{
public:

  ExpressionBase(const ExprT& expr) :
    m_constant_values(),
    m_expr( DeepCopy()( ReplaceConfigurableConstants()(expr, m_constant_values) ) )
  {
    // Store the variables
    CopyNumberedVars<VariablesT> ctx(m_variables);
    boost::proto::eval(expr, ctx);
  }

  void add_options(Common::OptionList& options)
  {
    // Add scalar options
    for(ConstantStorage::ScalarsT::iterator it = m_constant_values.m_scalars.begin(); it != m_constant_values.m_scalars.end(); ++it)
    {
      const std::string& name = it->first;
      Common::Option& option = options.check(name) ? options.option(name) : *options.add_option< Common::OptionT<Real> >(name, it->second);
      option.description(m_constant_values.descriptions[name]);
      option.link_to(&it->second);
    }

    // Add vector options
    for(ConstantStorage::VectorsT::iterator it = m_constant_values.m_vectors.begin(); it != m_constant_values.m_vectors.end(); ++it)
    {
      const std::string& name = it->first;
      Common::Option& option = options.check(name) ? options.option(name) : *options.add_option< Common::OptionT<RealVector> >(name, it->second);
      option.description(m_constant_values.descriptions[name]);
      option.link_to(&it->second);
    }
  }

  void register_variables(Physics::PhysModel& physical_model)
  {
    boost::fusion::for_each(m_variables, RegisterVariables(physical_model));
  }

private:
  /// Values for configurable constants
  ConstantStorage m_constant_values;
protected:

  /// Store a copy of the expression
  typedef typename boost::result_of< DeepCopy(typename boost::result_of<ReplaceConfigurableConstants(ExprT, ConstantStorage)>::type) >::type CopiedExprT;
  CopiedExprT m_expr;

  // Number of variables
  typedef typename ExpressionProperties<ExprT>::NbVarsT NbVarsT;

  // Type of a fusion vector that can contain a copy of each variable that is used in the expression
  typedef typename ExpressionProperties<ExprT>::VariablesT VariablesT;
  VariablesT m_variables;

  // True for the variables that are stored
  typedef typename EquationVariables<ExprT, NbVarsT>::type EquationVariablesT;

private:

  /// Functor to register variables in a physical model
  struct RegisterVariables
  {
    RegisterVariables(Physics::PhysModel& physical_model) :
      m_physical_model(physical_model)
    {
    }

    /// Register a scalar
    void operator()(ScalarField& field) const
    {
      get_descriptor(field.field_tag()).push_back(field.name(), Math::VariablesDescriptor::Dimensionalities::SCALAR);
    }

    /// Register a vector field
    void operator()(VectorField& field) const
    {
      get_descriptor(field.field_tag()).push_back(field.name(), Math::VariablesDescriptor::Dimensionalities::VECTOR);
    }

    /// Skip unused variables
    void operator()(const boost::mpl::void_&) const
    {
    }

  private:
    /// Get the VariablesDescriptor with the given tag
    Math::VariablesDescriptor& get_descriptor(const std::string& tag) const
    {
      Math::VariablesDescriptor* result = 0;
      BOOST_FOREACH(Math::VariablesDescriptor& descriptor, Common::find_components_with_tag<Math::VariablesDescriptor>(m_physical_model.variable_manager(), tag))
      {
        if(is_not_null(result))
          throw Common::SetupError(FromHere(), "Variablemanager " + m_physical_model.variable_manager().uri().string() + " has multiple descriptors with tag " + tag);
        result = &descriptor;
      }

      if(is_null(result))
      {
        result = &m_physical_model.variable_manager().create_component<Math::VariablesDescriptor>(tag);
        result->add_tag(tag);
      }

      return *result;
    }

    Physics::PhysModel& m_physical_model;
  };
};

template< typename ExprT, typename ElementTypes >
class ElementsExpression : public ExpressionBase<ExprT>
{
  typedef ExpressionBase<ExprT> BaseT;
public:

  ElementsExpression(const ExprT& expr) : BaseT(expr)
  {
  }

  void loop(Mesh::CRegion& region)
  {
    // Traverse all CElements under the region and evaluate the expression
    BOOST_FOREACH(Mesh::CElements& elements, Common::find_components_recursively<Mesh::CElements>(region) )
    {
      boost::mpl::for_each<boost::mpl::filter_view< ElementTypes, Mesh::IsMinimalOrder<1> > >( ElementLooper<ElementTypes, typename BaseT::CopiedExprT>(elements, BaseT::m_expr, BaseT::m_variables) );
    }
  }
};

/// Expression for looping over nodes
template<typename ExprT>
class NodesExpression : public ExpressionBase<ExprT>
{
  typedef ExpressionBase<ExprT> BaseT;
public:

  NodesExpression(const ExprT& expr) : BaseT(expr)
  {
  }

  void loop(Mesh::CRegion& region)
  {
    // IF COMPILATION FAILS HERE: the espression passed is invalid
    BOOST_MPL_ASSERT_MSG(
      (boost::proto::matches<typename BaseT::CopiedExprT, NodeGrammar>::value),
      INVALID_NODE_EXPRESSION,
      (NodeGrammar));

    boost::mpl::for_each< boost::mpl::range_c<Uint, 1, 4> >( NodeLooper<typename BaseT::CopiedExprT>(BaseT::m_expr, region, BaseT::m_variables) );

    // Synchronize fields if needed
    if(Common::PE::Comm::instance().is_active())
      boost::mpl::for_each< boost::mpl::range_c<Uint, 0, BaseT::NbVarsT::value> >(SynchronizeFields(BaseT::m_variables, region));
  }
private:
  /// Fusion functor to synchronize fields if needed
  struct SynchronizeFields
  {
    SynchronizeFields(const typename BaseT::VariablesT& vars, Mesh::CRegion& region) :
      m_variables(vars),
      m_region(region)
    {
    }

    template<typename VarIdxT>
    void operator()(const VarIdxT& i)
    {
      typedef typename boost::result_of<IsModified<VarIdxT::value>(ExprT)>::type IsModifiedT;
      apply(IsModifiedT(), i);
    }

    /// Do nothing if the variable is not modified
    template<typename VarIdxT>
    void apply(boost::mpl::false_, const VarIdxT&)
    {
    }

    /// Synchronize if modified
    template<typename VarIdxT>
    void apply(boost::mpl::true_, const VarIdxT&)
    {
      const std::string& tag = boost::fusion::at<VarIdxT>(m_variables).field_tag();
      Mesh::CMesh& mesh = Common::find_parent_component<Mesh::CMesh>(m_region);
      Mesh::Field& field = Common::find_component_recursively_with_tag<Mesh::Field>(mesh, tag);
      field.synchronize();
    }

    const typename BaseT::VariablesT& m_variables;
    Mesh::CRegion& m_region;
  };
};

/// Default element types supported by elements expressions
typedef Mesh::LagrangeP1::CellTypes DefaultElementTypes;

/// Convenience method to construct an Expression to loop over elements
/// @returns a shared pointer to the constructed expression
template<typename ExprT, typename ElementTypes>
boost::shared_ptr< ElementsExpression<ExprT, ElementTypes> > elements_expression(ElementTypes, const ExprT& expr)
{
  return boost::shared_ptr< ElementsExpression<ExprT, ElementTypes> >(new ElementsExpression<ExprT, ElementTypes>(expr));
}

/// Convenience method to construct an Expression to loop over elements
/// @returns a shared pointer to the constructed expression
template<typename ExprT>
boost::shared_ptr< ElementsExpression<ExprT, DefaultElementTypes> > elements_expression(const ExprT& expr)
{
  return elements_expression(DefaultElementTypes(), expr);
}

/// Convenience method to construct an Expression to loop over elements
/// @returns a shared pointer to the constructed expression
template<typename ExprT>
boost::shared_ptr< NodesExpression<ExprT> > nodes_expression(const ExprT& expr)
{
  return boost::shared_ptr< NodesExpression<ExprT> >(new NodesExpression<ExprT>(expr));
}

} // namespace Proto
} // namespace Actions
} // namespace Solver
} // namespace CF

#endif // CF_Solver_Actions_Proto_Expression_hpp
