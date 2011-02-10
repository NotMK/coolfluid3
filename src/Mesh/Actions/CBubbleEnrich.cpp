// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <boost/foreach.hpp>

#include "Common/Log.hpp"
#include "Common/CBuilder.hpp"
#include "Common/ComponentPredicates.hpp"
#include "Common/Foreach.hpp"
#include "Common/StreamHelpers.hpp"

#include "Mesh/Actions/CBubbleEnrich.hpp"
#include "Mesh/CCells.hpp"
#include "Mesh/CNodes.hpp"
#include "Mesh/CRegion.hpp"
#include "Mesh/CField2.hpp"
#include "Mesh/CFaceCellConnectivity.hpp"

#include "Math/MathFunctions.hpp"

//////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace Mesh {
namespace Actions {
  
  using namespace Common;
  using namespace Math::MathFunctions;
  
////////////////////////////////////////////////////////////////////////////////

Common::ComponentBuilder < CBubbleEnrich, CMeshTransformer, LibActions> CBubbleEnrich_Builder;

//////////////////////////////////////////////////////////////////////////////

CBubbleEnrich::CBubbleEnrich( const std::string& name )
: CMeshTransformer(name)
{
   
  properties()["brief"] = std::string("Enriches a Lagrangian space with bubble functions in each element");
  properties()["description"] = std::string("  Usage: CBubbleEnrich");
}

/////////////////////////////////////////////////////////////////////////////

std::string CBubbleEnrich::brief_description() const
{
  return properties()["brief"].value<std::string>();
}

/////////////////////////////////////////////////////////////////////////////

  
std::string CBubbleEnrich::help() const
{
  return "  " + properties()["brief"].value<std::string>() + "\n" + properties()["description"].value<std::string>();
}  
  
/////////////////////////////////////////////////////////////////////////////

void CBubbleEnrich::transform( const CMesh::Ptr& meshptr,
                               const std::vector<std::string>& args)
{
  CFinfo << "---------------------------------------------------" << CFendl;
  CFinfo << FromHere().str() << CFendl;

  /// @todo only supports simplexes

  CMesh& mesh = *meshptr;

  // print connectivity
//  boost_foreach(CElements& elements, find_components_recursively<CCells>(mesh))
//  {
//    CFinfo << "---------------------------------------------------" << CFendl;
//    CFinfo << elements.connectivity_table().full_path().string()  << CFendl;    // loop on the elements
//    for ( Uint elem = 0; elem != elements.connectivity_table().size(); ++elem )
//    {
//      for ( Uint n = 0; n != elements.connectivity_table().row_size(); ++n )
//        CFinfo << " " << elements.connectivity_table().array()[elem][n];
//      CFinfo << CFendl;
//    }
//  }


//  boost_foreach(CRegion& region, find_components_recursively<CRegion>(mesh) )
  boost_foreach(CElements& elements, find_components_recursively<CCells>(mesh))
  {     
    CFinfo << "---------------------------------------------------" << CFendl;

    CFinfo << elements.full_path().string() << CFendl;

//    CFinfo << "elems size " << elements.size() << CFendl;
//    CFinfo << "conn size "  << elements.connectivity_table().size() << " x "
//                            << elements.connectivity_table().row_size() << CFendl;
//    CFinfo << "coords size "<< elements.nodes().coordinates().size() << " x "
//                            << elements.nodes().coordinates().row_size() << CFendl;

    // backup the connectivity table
    CTable<Uint>::Ptr backup = this->create_component< CTable<Uint> > ("backup");

    backup->set_row_size(elements.connectivity_table().row_size()); // size appropriately
    backup->resize(elements.connectivity_table().size());           // size appropriately

    *backup = elements.connectivity_table(); /* copy table */
    CTable<Uint>::ArrayT& backtable = backup->array();

    // compute new nb cols
    const Uint currnodes = elements.connectivity_table().row_size();
    const Uint newnbcols = currnodes + 1;
    // resize and keep same nb of rows (nb elements)
    elements.connectivity_table().set_row_size(newnbcols);

//    CFinfo << "new conn size "  << elements.connectivity_table().size() << " x "
//                                << elements.connectivity_table().row_size() << CFendl;

    // get connectivity table
    CTable<Uint>::ArrayT& conntable = elements.connectivity_table().array();

    // get coordinates
    CTable<Real>& coords = elements.nodes().coordinates();

    const Uint nb_elem = elements.size();
    const Uint dim = coords.row_size();

    // average coordinate
    RealVector centroid (dim);


    // get a buffer to the coordinates
    CTable<Real>::Buffer buf = coords.create_buffer();

    // loop on the elements
    for ( Uint elem = 0; elem != nb_elem; ++elem )
    {
      centroid.setZero();

      // compute average of node coordinates
      for ( Uint n = 0; n != dim+1; ++n )
      {
        const Uint nodeid = backtable[elem][n];
        for ( Uint d = 0; d != dim; ++d )
         centroid[d] += coords[nodeid][d];
      }
      centroid /= dim+1;

      // add a node to the nodes structure
      Uint idx = buf.add_row( centroid );

      // add the index of the node to the connectivity table
      for ( Uint n = 0; n != currnodes; ++n )
        conntable[elem][n] = backtable[elem][n];
      conntable[elem][currnodes] = idx;

    }

    // flush the coordinate buffer
    buf.flush();

    // delete backup table
    this->remove_component("backup");

    // remove the old shape function
    const ElementType& etype = elements.element_type();
    elements.remove_component(etype.name());

    // add the new shape function
    elements.set_element_type( "CF.Mesh.SF.Triag2DLagrangeP2B" );

  } // loop element types

  // print connectivity
//  boost_foreach(CElements& elements, find_components_recursively<CCells>(mesh))
//  {
//    CFinfo << "---------------------------------------------------" << CFendl;
//    CFinfo << elements.connectivity_table().full_path().string()  << CFendl;    // loop on the elements
//    for ( Uint elem = 0; elem != elements.connectivity_table().size(); ++elem )
//    {
//      for ( Uint n = 0; n != elements.connectivity_table().row_size(); ++n )
//        CFinfo << " " << elements.connectivity_table().array()[elem][n];
//      CFinfo << CFendl;
//    }
//  }

}

//////////////////////////////////////////////////////////////////////////////

} // Actions
} // Mesh
} // CF