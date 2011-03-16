// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_UFEM_LinearSystem_hpp
#define CF_UFEM_LinearSystem_hpp

#include "Common/OptionURI.hpp"

#include "Solver/CEigenLSS.hpp"
#include "Solver/CMethod.hpp"
#include "Solver/Actions/CFieldAction.hpp"
#include "Solver/Actions/Proto/PhysicalModel.hpp"
#include "Solver/Actions/Proto/Terminals.hpp"

#include "LibUFEM.hpp"

namespace CF {

namespace UFEM {

/// A method that solves a linear system to obtain the problem solution. The system is set up based on a proto expression
class UFEM_API LinearSystem : public Solver::CMethod
{
public: // typedefs

  typedef boost::shared_ptr<LinearSystem> Ptr;
  typedef boost::shared_ptr<LinearSystem const> ConstPtr;

public: // functions

  /// Contructor
  /// @param name of the component
  LinearSystem ( const std::string& name );

  /// Get the class name
  static std::string type_name () { return "LinearSystem"; }

  /// The linear system solver
  Solver::CEigenLSS& lss();

  /// @name SIGNALS
  //@{

  /// Signal to run the model
  void run(Common::SignalArgs& node);

  /// Signal to add Dirichlet boundary conditions
  void add_dirichlet_bc( Common::SignalArgs& node );

  /// Signature for the add_dirichlet_bc ssignal
  void dirichlet_bc_signature( Common::SignalArgs& node);

  /// Signal to add an intial condition
  void add_initial_condition( Common::SignalArgs& node );

  /// Signature for add_initial_condition
  void add_initial_condition_signature(Common::SignalArgs& node);

  /// Signal to run the intialization phase
  void signal_initialize_fields(Common::SignalArgs& node);

  //@} END SIGNALS
protected:
  /// Override this to return a CAction that will build the linear system
  virtual Solver::Actions::CFieldAction::Ptr build_equation() = 0;

  /// Overridable implementation that is called when the action is run
  virtual void on_run();
  
  /// Adds a configurable constant
  Solver::Actions::Proto::StoredReference<Real> add_configurable_constant(const std::string& name, const std::string& description, const Real default_value);
  
  /// Temp storage for configurable constants that are added
  typedef std::map<std::string, Common::Component::Ptr> ConstantsT;
  ConstantsT m_configurable_constants;

  /// Called when the LSS is changed
  void on_lss_set();

  /// Path to the linear system
  boost::weak_ptr<Common::OptionURI> m_lss_path;

  /// Linear system
  boost::weak_ptr<Solver::CEigenLSS> m_lss;

  /// Action that will set up the linear system
  boost::weak_ptr<Solver::Actions::CFieldAction> m_system_builder;
  
  Solver::Actions::Proto::PhysicalModel m_physical_model;
};

} // UFEM
} // CF


#endif // CF_UFEM_LinearSystem_hpp
